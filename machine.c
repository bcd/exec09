
#include <stdio.h>
#include <string.h>
#include "machine.h"

#define MISSING 0xff
#define mmu_device (device_table[0])

unsigned int device_count = 0;
struct hw_device *device_table[MAX_BUS_DEVICES];

struct bus_map busmaps[NUM_BUS_MAPS];


/**
 * Attach a new device to the bus.  Only called during init.
 */
struct hw_device *device_attach (struct hw_class *class_ptr, unsigned int size, void *priv)
{
	struct hw_device *dev = malloc (sizeof (struct hw_device));
	dev->class_ptr = class_ptr;
	dev->devid = device_count;
	dev->size = size;
	dev->priv = priv;
	device_table[device_count++] = dev;

	/* Attach implies reset */
	class_ptr->reset (dev);

	return dev;
};


/**
 * Map a portion of a device into the CPU's address space.
 */
void bus_map (unsigned int addr,
	unsigned int devid,
	unsigned long offset,
	unsigned int len)
{
	struct bus_map *map;
	unsigned int start, count;

	/* Round address and length to be multiples of the map unit size. */
	addr = ((addr + BUS_MAP_SIZE - 1) / BUS_MAP_SIZE) * BUS_MAP_SIZE;
	len = ((len + BUS_MAP_SIZE - 1) / BUS_MAP_SIZE) * BUS_MAP_SIZE;
	offset = ((offset + BUS_MAP_SIZE - 1) / BUS_MAP_SIZE) * BUS_MAP_SIZE;

	/* Convert from byte addresses to unit counts */
	start = addr / BUS_MAP_SIZE;
	count = len / BUS_MAP_SIZE;

	/* Initialize the maps.  This will let the CPU access the device. */
	map = &busmaps[start];
	while (count > 0)
	{
		map->devid = devid;
		map->offset = offset;
		count--;
		map++;
		offset += BUS_MAP_SIZE;
	}
}


static struct bus_map *find_map (unsigned int addr)
{
	return &busmaps[addr / BUS_MAP_SIZE];
}


void dump_machine (void)
{
	unsigned int mapno;

	for (mapno = 0; mapno < NUM_BUS_MAPS; mapno++)
	{
		struct bus_map *map = &busmaps[mapno];
		printf ("Map %d  addr=%04X  dev=%d  offset=%04X  size=%06X\n",
			mapno, mapno * BUS_MAP_SIZE, map->devid, map->offset,
			device_table[map->devid]->size);
	}
}


/**
 * Called by the CPU to read a byte.
 */
U8 cpu_read8 (unsigned int addr)
{
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = device_table[map->devid];
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;
	return (*class_ptr->read) (dev, phy_addr);
}


/**
 * Called by the CPU to read a byte.
 */
void cpu_write8 (unsigned int addr, U8 val)
{
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = device_table[map->devid];
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;
	(*class_ptr->write) (dev, phy_addr, val);
}

/**********************************************************/

void null_reset (struct hw_device *dev)
{
}

U8 null_read (struct hw_device *dev, unsigned long addr)
{
	return 0;
}

void null_write (struct hw_device *dev, unsigned long addr, U8 val)
{
}

/**********************************************************/

void ram_reset (struct hw_device *dev)
{
	memset (dev->priv, 0, dev->size);
}

U8 ram_read (struct hw_device *dev, unsigned long addr)
{
	char *buf = dev->priv;
	return buf[addr];
}

void ram_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	char *buf = dev->priv;
	buf[addr] = val;
}

struct hw_class ram_class =
{
	.reset = ram_reset,
	.read = ram_read,
	.write = ram_write,
};

void ram_init (unsigned long size)
{
	void *buf = malloc (size);
	struct hw_device *dev = device_attach (&ram_class, size, buf);
	/* Map the RAM into the entire address space of the 6809.
	Other devices that get attached may override this. */
	bus_map (0x0000, dev->devid, 0, 0x10000);
}

/**********************************************************/

struct hw_class rom_class =
{
	.reset = null_reset,
	.read = ram_read,
	.write = null_write,
};

void rom_init (const char *filename)
{
	FILE *fp = fopen (filename, "rb");
	if (fp)
	{
		unsigned int size = sizeof_file (fp);
		char *buf = malloc (size);
		fread (buf, size, 1, fp);
		struct hw_device *dev = device_attach (&rom_class, size, buf);
		bus_map (BOOT_ROM_ADDR, dev->devid, 0, 0x2000);
	}
	fclose (fp);
}

/**********************************************************/

#define CON_OUT 0
#define CON_EXIT 1
#define CON_IN 2

U8 console_read (struct hw_device *dev, unsigned long addr)
{
	switch (addr)
	{
		case CON_IN:
			return getchar ();
		default:
			return MISSING;
	}
}

void console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	switch (addr)
	{
		case CON_OUT:
			putchar (val);
			break;
		case CON_EXIT:
			sim_exit (val);
			break;
		default:
			break;
	}
}

struct hw_class console_class =
{
	.reset = null_reset,
	.read = console_read,
	.write = console_write,
};

void console_init (void)
{
	struct hw_device *dev = device_attach (&console_class, BUS_MAP_SIZE, NULL);
	bus_map (CONSOLE_ADDR, dev->devid, 0, BUS_MAP_SIZE);

	/* For legacy code */
	bus_map (0xff00, dev->devid, 0, BUS_MAP_SIZE);
}

/**********************************************************/

#define PAGE_SIZE 8192
#define PAGE_COUNT 8
#define PAGE_REGS 8

U8 mmu_regs[PAGE_COUNT][PAGE_REGS];

U8 mmu_read (struct hw_device *dev, unsigned long addr)
{
	unsigned int page = (addr / PAGE_REGS) % PAGE_COUNT;
	unsigned int reg = addr % PAGE_REGS;
	return mmu_regs[page][reg];
}

void mmu_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	unsigned int page = (addr / PAGE_REGS) % PAGE_COUNT;
	unsigned int reg = addr % PAGE_REGS;
	mmu_regs[page][reg] = val;
}

void mmu_reset (struct hw_device *dev)
{
	unsigned int page;
	for (page = 0; page < PAGE_COUNT; page++)
	{
		mmu_write (dev, MMU_ADDR + (page * PAGE_REGS) + 0, 0);
		mmu_write (dev, MMU_ADDR + (page * PAGE_REGS) + 1, 0);
		mmu_write (dev, MMU_ADDR + (page * PAGE_REGS) + 2, 0);
	}
}

struct hw_class mmu_class =
{
	.reset = mmu_reset,
	.read = mmu_read,
	.write = mmu_write,
};

void mmu_init (void)
{
	struct hw_device *dev = device_attach (&mmu_class, BUS_MAP_SIZE, NULL);
	bus_map (MMU_ADDR, dev->devid, 0, BUS_MAP_SIZE);
}

/**********************************************************/

void machine_init (const char *boot_rom_file)
{
	/* The MMU must be initialized first, as all other devices
	that are attached can try to hook into the MMU. */
	mmu_init ();
	ram_init (0x100000); /* 1MB */

	if (boot_rom_file)
		rom_init (boot_rom_file);

	console_init ();

	dump_machine ();
}

