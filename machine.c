/*
 * Copyright 2008 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of GCC6809.
 *
 * GCC6809 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GCC6809 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GCC6809; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "machine.h"
#include "6809.h"
#include "eon.h"

#define CONFIG_LEGACY
#define MISSING 0xff
#define mmu_device (device_table[0])

extern void eon_init (const char *);
extern void wpc_init (const char *);

struct machine *machine;

unsigned int device_count = 0;
struct hw_device *device_table[MAX_BUS_DEVICES];

struct hw_device *null_device;

struct bus_map busmaps[NUM_BUS_MAPS];

struct bus_map default_busmaps[NUM_BUS_MAPS];

U16 fault_addr;

U8 fault_type;

int system_running = 0;


void cpu_is_running (void)
{
	system_running = 1;
}

void do_fault (unsigned int addr, unsigned int type)
{
	if (system_running)
		machine->fault (addr, type);
}


void exit_fault (unsigned int addr, unsigned int type)
{
	monitor_on = debug_enabled;
	sim_error ("Fault: addr=%04X type=%02X\n", addr, type);
	exit (1);
}


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
	unsigned int len,
	unsigned int flags)
{
	struct bus_map *map;
	unsigned int start, count;

	/* Warn if trying to map too much */
	if (addr + len > MAX_CPU_ADDR)
	{
		fprintf (stderr, "warning: mapping %04X bytes into %04X causes overflow\n",
			len, addr);
	}

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
		if (!(map->flags & MAP_FIXED))
		{
			map->devid = devid;
			map->offset = offset;
			map->flags = flags;
		}
		count--;
		map++;
		offset += BUS_MAP_SIZE;
	}
}

void device_define (struct hw_device *dev,
	unsigned long offset,
	unsigned int addr,
	unsigned int len,
	unsigned int flags)
{
	/* Note: len must be a multiple of BUS_MAP_SIZE */
	bus_map (addr, dev->devid, offset, len, flags);
}


void bus_unmap (unsigned int addr, unsigned int len)
{
	struct bus_map *map;
	unsigned int start, count;

	/* Round address and length to be multiples of the map unit size. */
	addr = ((addr + BUS_MAP_SIZE - 1) / BUS_MAP_SIZE) * BUS_MAP_SIZE;
	len = ((len + BUS_MAP_SIZE - 1) / BUS_MAP_SIZE) * BUS_MAP_SIZE;

	/* Convert from byte addresses to unit counts */
	start = addr / BUS_MAP_SIZE;
	count = len / BUS_MAP_SIZE;

	/* Set the maps to their defaults. */
	memcpy (&busmaps[start], &default_busmaps[start],
		sizeof (struct bus_map) * count);
}


/**
 * Generate a page fault.  ADDR says which address was accessed
 * incorrectly.  TYPE says what kind of violation occurred.
 */

static struct bus_map *find_map (unsigned int addr)
{
	return &busmaps[addr / BUS_MAP_SIZE];
}

static struct hw_device *find_device (unsigned int addr, unsigned char id)
{
	/* Fault if any invalid device is accessed */
	if ((id == INVALID_DEVID) || (id >= device_count))
	{
		do_fault (addr, FAULT_NO_RESPONSE);
		return null_device;
	}
	return device_table[id];
}


void
print_device_name (unsigned int devno)
{
   struct hw_device *dev = device_table[devno];
   printf ("%02X", devno);
}


absolute_address_t
absolute_from_reladdr (unsigned int device, unsigned long reladdr)
{
   return (device * 0x10000000L) + reladdr;
}


U8 abs_read8 (absolute_address_t addr)
{
   unsigned int id = addr >> 28;
   unsigned long phy_addr = addr & 0xFFFFFFF;
	struct hw_device *dev = device_table[id];
	struct hw_class *class_ptr = dev->class_ptr;
	return (*class_ptr->read) (dev, phy_addr);
}


/**
 * Called by the CPU to read a byte.
 * This is the bottleneck in terms of performance.  Consider
 * a caching scheme that cuts down on some of this.
 * There is also a 16-bit version that is more efficient when
 * a full word is needed, but it implies that no reads will ever
 * occur across a device boundary.
 */
U8 cpu_read8 (unsigned int addr)
{
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = find_device (addr, map->devid);
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;

	if (system_running && !(map->flags & MAP_READABLE))
		machine->fault (addr, FAULT_NOT_READABLE);
	command_read_hook (absolute_from_reladdr (map->devid, phy_addr));
	return (*class_ptr->read) (dev, phy_addr);
}

U16 cpu_read16 (unsigned int addr)
{
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = find_device (addr, map->devid);
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;

	if (system_running && !(map->flags & MAP_READABLE))
		do_fault (addr, FAULT_NOT_READABLE);
	command_read_hook (absolute_from_reladdr (map->devid, phy_addr));
	return ((*class_ptr->read) (dev, phy_addr) << 8)
			| (*class_ptr->read) (dev, phy_addr+1);
}


/**
 * Called by the CPU to write a byte.
 */
void cpu_write8 (unsigned int addr, U8 val)
{
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = find_device (addr, map->devid);
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;

	if (system_running && !(map->flags & MAP_WRITABLE))
		do_fault (addr, FAULT_NOT_WRITABLE);
	(*class_ptr->write) (dev, phy_addr, val);
	command_write_hook (absolute_from_reladdr (map->devid, phy_addr), val);
}

void abs_write8 (absolute_address_t addr, U8 val)
{
   unsigned int id = addr >> 28;
   unsigned long phy_addr = addr & 0xFFFFFFF;
	struct hw_device *dev = device_table[id];
	struct hw_class *class_ptr = dev->class_ptr;
	class_ptr->write (dev, phy_addr, val);
}



absolute_address_t
to_absolute (unsigned long cpuaddr)
{
	struct bus_map *map = find_map (cpuaddr);
	struct hw_device *dev = find_device (cpuaddr, map->devid);
	unsigned long phy_addr = map->offset + cpuaddr % BUS_MAP_SIZE;
	return absolute_from_reladdr (map->devid, phy_addr);
}


void dump_machine (void)
{
	unsigned int mapno;
	unsigned int n;

	for (mapno = 0; mapno < NUM_BUS_MAPS; mapno++)
	{
		struct bus_map *map = &busmaps[mapno];
		printf ("Map %d  addr=%04X  dev=%d  offset=%04X  size=%06X  flags=%02X\n",
			mapno, mapno * BUS_MAP_SIZE, map->devid, map->offset,
			0 /* device_table[map->devid]->size */, map->flags);

#if 0
		for (n = 0; n < BUS_MAP_SIZE; n++)
			printf ("%02X ", cpu_read8 (mapno * BUS_MAP_SIZE + n));
		printf ("\n");
#endif
	}
}


/**********************************************************/

void null_reset (struct hw_device *dev)
{
}

U8 null_read (struct hw_device *dev, unsigned long addr)
{
	return 0xFF;
}

void null_write (struct hw_device *dev, unsigned long addr, U8 val)
{
}

struct hw_class null_class =
{
	.readonly = 0,
	.reset = null_reset,
	.read = null_read,
	.write = null_write,
};

struct hw_device *null_create (void)
{
	return device_attach (&null_class, 0, NULL);
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
	.readonly = 0,
	.reset = ram_reset,
	.read = ram_read,
	.write = ram_write,
};

struct hw_device *ram_create (unsigned long size)
{
	void *buf = malloc (size);
	return device_attach (&ram_class, size, buf);
}

/**********************************************************/

struct hw_class rom_class =
{
	.readonly = 1,
	.reset = null_reset,
	.read = ram_read,
	.write = ram_write,
};


struct hw_device *rom_create (const char *filename, unsigned int maxsize)
{
	FILE *fp;
	struct hw_device *dev;
	unsigned int image_size;
	char *buf;

	if (filename)
	{	
		fp = file_open (NULL, filename, "rb");
		if (!fp)
			return NULL;
		image_size = sizeof_file (fp);
	}

	buf = malloc (maxsize);
	dev = device_attach (&rom_class, maxsize, buf);
	if (filename)
	{
		fread (buf, image_size, 1, fp);
		fclose (fp);
		maxsize -= image_size;
		while (maxsize > 0)
		{
			memcpy (buf + image_size, buf, image_size);
			buf += image_size;
			maxsize -= image_size;
		}
	}

	return dev;
}

/**********************************************************/

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
	.readonly = 0,
	.reset = null_reset,
	.read = console_read,
	.write = console_write,
};

struct hw_device *console_create (void)
{
	return device_attach (&console_class, BUS_MAP_SIZE, NULL);
}

/**********************************************************/


U8 mmu_regs[MMU_PAGECOUNT][MMU_PAGEREGS];

U8 mmu_read (struct hw_device *dev, unsigned long addr)
{
	switch (addr)
	{
		case 0x60:
			return fault_addr >> 8;
		case 0x61:
			return fault_addr & 0xFF;
		case 0x62:
			return fault_type;
		default:
		{
			unsigned int page = (addr / MMU_PAGEREGS) % MMU_PAGECOUNT;
			unsigned int reg = addr % MMU_PAGEREGS;
			U8 val = mmu_regs[page][reg];
			//printf ("\n%02X, %02X = %02X\n", page, reg, val);
			return val;
		}
	}
}

void mmu_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	unsigned int page = (addr / MMU_PAGEREGS) % MMU_PAGECOUNT;
	unsigned int reg = addr % MMU_PAGEREGS;
	mmu_regs[page][reg] = val;

	bus_map (page * MMU_PAGESIZE,
	         mmu_regs[page][0],
				mmu_regs[page][1] * MMU_PAGESIZE,
				MMU_PAGESIZE,
				mmu_regs[page][2] & MAP_READWRITE);
}

void mmu_reset (struct hw_device *dev)
{
	unsigned int page;
	for (page = 0; page < MMU_PAGECOUNT; page++)
	{
		mmu_write (dev, page * MMU_PAGEREGS + 0, 0);
		mmu_write (dev, page * MMU_PAGEREGS + 1, 0);
		mmu_write (dev, page * MMU_PAGEREGS + 2, MAP_READWRITE);
	}
}

void mmu_reset_complete (struct hw_device *dev)
{
	unsigned int page;
	const struct bus_map *map;

	/* Examine all of the bus_maps in place now, and
	sync with the MMU registers. */
	for (page = 0; page < MMU_PAGECOUNT; page++)
	{
		map = &busmaps[4 + page * (MMU_PAGESIZE / BUS_MAP_SIZE)];
		mmu_regs[page][0] = map->devid;
		mmu_regs[page][1] = map->offset / MMU_PAGESIZE;
		mmu_regs[page][2] = map->flags & MAP_READWRITE;
		/* printf ("%02X %02X %02X\n",
			map->devid, map->offset / MMU_PAGESIZE,
			map->flags); */
	}
}


struct hw_class mmu_class =
{
	.readonly = 0,
	.reset = mmu_reset,
	.read = mmu_read,
	.write = mmu_write,
};

struct hw_device *mmu_create (void)
{
	return device_attach (&mmu_class, BUS_MAP_SIZE, NULL);
}


/**********************************************************/

void machine_update (void)
{
	int i;
	for (i=0; i < device_count; i++)
	{
		struct hw_device *dev = device_table[i];
		if (dev->class_ptr->update)
			dev->class_ptr->update (dev);
	}
}

int machine_match (const char *machine_name, const char *boot_rom_file, struct machine *m)
{
	if (!strcmp (m->name, machine_name))
	{
		machine = m;
		m->init (boot_rom_file);
		return 1;
	}
	return 0;
}


void machine_init (const char *machine_name, const char *boot_rom_file)
{
	extern struct machine simple_machine;
	extern struct machine eon_machine;
	extern struct machine eon2_machine;
	extern struct machine wpc_machine;
	int i;

	/* Initialize CPU maps, so that no CPU addresses map to
	anything.  Default maps will trigger faults at runtime. */
	null_device = null_create ();
	memset (busmaps, 0, sizeof (busmaps));
	for (i = 0; i < NUM_BUS_MAPS; i++)
		busmaps[i].devid = INVALID_DEVID;

	if (machine_match (machine_name, boot_rom_file, &simple_machine));
	else if (machine_match (machine_name, boot_rom_file, &eon_machine));
	else if (machine_match (machine_name, boot_rom_file, &eon2_machine));
	else if (machine_match (machine_name, boot_rom_file, &wpc_machine));
	else exit (1);

	/* Save the default busmap configuration, before the
	CPU begins to run, so that it can be restored if
	necessary. */
	memcpy (default_busmaps, busmaps, sizeof (busmaps));

	if (!strcmp (machine_name, "eon"))
		mmu_reset_complete (mmu_device);
}

