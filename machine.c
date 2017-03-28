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
#include "command.h"
#include "monitor.h"
#include "6809.h"
#include "eon.h"

#define CONFIG_LEGACY
#define MISSING 0xff
#define mmu_device (device_table[0])

extern FILE *log_file;
struct machine *machine;

unsigned int device_count = 0;
struct hw_device *device_table[MAX_BUS_DEVICES];

struct hw_device *null_device;

struct bus_map busmaps[NUM_BUS_MAPS];

struct bus_map default_busmaps[NUM_BUS_MAPS];

U16 fault_addr;

U8 fault_type;

/* set after CPU reset and never cleared; shows that
   system initialisation has completed */
int cpu_running = 0;

void cpu_is_running (void)
{
	cpu_running = 1;
}

void do_fault (unsigned int addr, unsigned int type)
{
	if (cpu_running)
		machine->fault (addr, type);
}

// nac never used.
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
}

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
		fprintf(stderr, "warning: mapping %04X bytes into %04X causes overflow\n",
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
	bus_map(addr, dev->devid, offset, len, flags);
}

void bus_unmap (unsigned int addr, unsigned int len)
{
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

void print_device_name (unsigned int devno)
{
   printf ("%02X", devno);
}

absolute_address_t absolute_from_reladdr (unsigned int device, unsigned long reladdr)
{
   return (device * 0x10000000L) + reladdr;
}

U8 abs_read8 (absolute_address_t addr)
{
	// nac come here on dbg examine. Core dump on access to nxm.
	// nac what is "id" and how is it extracted from top 4 bits and
	// why does this need addr to be 64 bits?
	unsigned int id = addr >> 28;
	unsigned long phy_addr = addr & 0xFFFFFFF;
	// printf("In abs_read8 with address 0x%x, id 0x%x and phy_addr 0x%x\n",addr,id,phy_addr);
        // nac BUG! should not be doing this directly:
        // an attempt to access a non-existent location (a location whose device ID is FF)
        // results in an attempt to access a non-existent value in the device_table.
        // Actually it's doubly bad: there are 32 devices (max) but access to non-existent
        // device seems to yield an ID of 0xf rather than 0xff which, based on the 28-bit
        // shift, implies that the address was bad in the first place: it only had f instead of ff
        // In any case, the table should not be indexed with f or ff becasue neither are valid
        // devices.
        // BUT! my "fix" below is bad; the fault gets reported 2ce and
        // the data value gets reported as a 32-bit value instead of a U8
        // eg, if null_read is set up to return 0xab it returns 0xffffffab
        // and if it's set up to return 0x3b it returns 0x3b -- ie, it is
        // being sign extended. Not sure why, though, becasue it looks identical
        // to the normal read; must be due to a path taken in the error handling?
        //
        // 2 scenarios: access to 0:0 ie direct access device 0 even though
        // that device is not mapped into the bus anywhere. Currently does not
        // report any error but does return 0xffffffff (sign extended). Not reporting an error
        // is fine (I suppose) because the access is not really being checked
        // -- it doesn't correspond to a CPU address.
        // Other scenario is access to 0x7c80 in smii. This is a real CPU
        // address but is mapped to a non-existent device. Currently get 2
        // errors reported: the first is due to an access through cpu_read8
        // and the error is a page fault, address 0x7c80 -- the error is
        // triggered by a check of the map entry. The second is due
        // to an access through abs_read8 and the error is a page fault,
        // address 0xf000.0000 -- thinks it's device 0xff but truncated.
        // For this one the data comes back as 0xffffffff (sign-extended).
        //
        // Maybe need to switch to using CPU addresses everywhere user-facing
        // and allow device:offset addressing only on the command line?
        //
        // Another option that might help a fix is to switch to initialising
        // the map with device 0 rather than with non-such-device. Then,
        // no-such-device can be a more fatal error...

        // orig: -- core dumps!!
        //struct hw_device *dev = device_table[id];
        // replacement: -- still not right.
	struct hw_device *dev = find_device (addr, id);

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

	if (cpu_running && !(map->flags & MAP_READABLE))
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

	if (cpu_running && !(map->flags & MAP_READABLE))
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
        //printf("write 0x%04x<-0x%02x\n", addr, val);
        //fprintf(log_file,"wr 0x%04x<-0x%02x\n", addr, val);
	struct bus_map *map = find_map (addr);
	struct hw_device *dev = find_device (addr, map->devid);
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;

        /* Unlike the read case, where we still return data on
           an access error, ignore write data on access error.
           The cpu_running check allows ROMs to be loaded at
           startup (but maybe it would be better if ROM load
           used absolute access so that this routine was not
           used at all for that purpose) */
	if (!cpu_running || (map->flags & MAP_WRITABLE))
            {
                (*class_ptr->write) (dev, phy_addr, val);
            }
        else if (map->flags & MAP_IGNOREWRITE)
            {
                /* silently ignore the write */
            }
        else if (cpu_running)
            {
		do_fault (addr, FAULT_NOT_WRITABLE);
            }
        /* do this regardless (may trigger watchpoint) */
        command_write_hook (absolute_from_reladdr (map->devid, phy_addr), val);
}

void abs_write8 (absolute_address_t addr, U8 val)
{
	unsigned int id = addr >> 28;
	unsigned long phy_addr = addr & 0xFFFFFFF;
	struct hw_device *dev = device_table[id];
	struct hw_class *class_ptr = dev->class_ptr;
	class_ptr->write(dev, phy_addr, val);
}

absolute_address_t to_absolute (unsigned long cpuaddr)
{
	/* if it's greater than 0xffff, it's already absolute
           and we cannot convert it again. If it's less than
           0x10000 it might already be absolute but it's safe
           to convert it a second time.
        */
	if (cpuaddr > 0xffff) return (absolute_address_t)cpuaddr;

	struct bus_map *map = find_map (cpuaddr);
	unsigned long phy_addr = map->offset + cpuaddr % BUS_MAP_SIZE;
	return absolute_from_reladdr (map->devid, phy_addr);
}


// Dump machine (if supported)
void dump_machine(void)
{
    if (machine->dump) {
        machine->dump();
    }
    else {
        printf("This machine does not provide a dump capability\n");
    }
}

// Describe machine, devices and mapping.
void describe_machine (void)
{
	unsigned int devno;
	unsigned int mapno;
	unsigned int prev_devid = -1;
	unsigned int prev_offset = 0;
	unsigned int prev_flags = 0;
	unsigned int dot_dot = 0;

	/* machine */
	printf("Machine: %s\n", machine->name);

	/* devices */
	for (devno = 0; devno < device_count; devno++)
	{
		printf("Device %2d: %s\n",devno, device_table[devno]->class_ptr->name);
	}

	/* mapping */
	for (mapno = 0; mapno < NUM_BUS_MAPS; mapno++)
	{
		struct bus_map *map = &busmaps[mapno];
		if ( (map->devid == prev_devid) && (map->flags == prev_flags) &&
                    ((map->offset == prev_offset) || (map->devid == INVALID_DEVID)) )
		{
			/* nothing interesting to report */
			if (! dot_dot)
			{
				printf("..\n");
				dot_dot = 1;
			}
		}
		else
		{
			dot_dot = 0;
                        printf ("Map %3d:  addr=%04X  dev=%d  offset=%04lX  size=%06lX  flags=%02X\n",
				mapno, mapno * BUS_MAP_SIZE, map->devid, map->offset,
				device_table[map->devid]->size, map->flags);
		}
		/* ready for next time */
		prev_devid = map->devid;
		prev_offset = map->offset + BUS_MAP_SIZE;
		prev_flags = map->flags;
	}
}

/**********************************************************
 * Simple fault handler
 **********************************************************/

void fault (unsigned int addr, unsigned char type)
{
	if (cpu_running)
	{
		sim_error (">>> Page fault: addr=%04X type=%02X PC=%04X\n", addr, type, get_pc ());
#if 0
		fault_addr = addr;
		fault_type = type;
		irq ();
#endif
	}
}

/**********************************************************/

void null_reset (struct hw_device *dev)
{
	(void) dev;	// silence warning unused parameter
}

U8 null_read (struct hw_device *dev, unsigned long addr)
{
	(void) dev;	// silence warning unused parameter
	(void) addr;

	return 0xFF;
}

void null_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	(void) dev;	// silence warning unused parameter
	(void) addr;
	(void) val;
}

struct hw_class null_class =
{
	.name = "null-device",
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
	.name = "RAM",
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
	.name = "ROM",
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
	.name = "console",
	.readonly = 0,
	.reset = null_reset,
	.read = console_read,
	.write = console_write,
        .update = NULL,
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
		mmu_regs[page][1] = (U8) (map->offset / MMU_PAGESIZE);
		mmu_regs[page][2] = map->flags & MAP_READWRITE;
		/* printf ("%02X %02X %02X\n",
			map->devid, map->offset / MMU_PAGESIZE,
			map->flags); */
	}
}

struct hw_class mmu_class =
{
	.name = "mmu",
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
	extern struct machine smii_machine;
	extern struct machine multicomp09_machine;
	extern struct machine kipper1_machine;
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
	else if (machine_match (machine_name, boot_rom_file, &smii_machine));
	else if (machine_match (machine_name, boot_rom_file, &multicomp09_machine));
	else if (machine_match (machine_name, boot_rom_file, &kipper1_machine));
	else exit (1);

	/* Save the default busmap configuration, before the
	CPU begins to run, so that it can be restored if
	necessary. */
	memcpy (default_busmaps, busmaps, sizeof (busmaps));

	if (!strcmp (machine_name, "eon"))
		mmu_reset_complete (mmu_device);
}

