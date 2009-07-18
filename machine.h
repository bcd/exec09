/*
 * Copyright 2006-2009 by Brian Dominy <brian@oddchange.com>
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

#ifndef M6809_MACHINE_H
#define M6809_MACHINE_H

/* This file defines structures used to build generic machines on a 6809. */

typedef unsigned char U8;
typedef unsigned short U16;

typedef unsigned long absolute_address_t;

#define MAX_CPU_ADDR 65536

/* The generic bus architecture. */

/* Up to 32 devices may be connected.  Each device is addressed by a 32-bit physical address */
#define MAX_BUS_DEVICES 32

#define INVALID_DEVID 0xff

/* Say whether or not the mapping is RO or RW (or neither). */
#define MAP_READABLE 0x1
#define MAP_WRITABLE 0x2
#define MAP_READWRITE 0x3

/* A fixed map cannot be reprogrammed.  Attempts to
bus_map something differently will silently be
ignored. */
#define MAP_FIXED 0x4

#define FAULT_NONE 0
#define FAULT_NOT_WRITABLE 1
#define FAULT_NO_RESPONSE 2
#define FAULT_NOT_READABLE 3

/* A bus map is assocated with part of the 6809 address space
and says what device and which part of it is mapped into that
area.  It also has associated flags which say how it is allowed
to be accessed.

A single bus map defines 128 bytes of address space; for a 64KB CPU,
that requires a total of 512 such structures.

Note that the bus map need not correspond to the page size that can
be configured by the MMU.  It allows for more granularity and is
needed in some *hardcoded* mapping cases. */

#define BUS_MAP_SIZE 128

struct bus_map
{
	unsigned int devid; /* The devid mapped here */
	unsigned long offset; /* The offset within the device */
	unsigned char flags;
};

#define NUM_BUS_MAPS (MAX_CPU_ADDR / BUS_MAP_SIZE)


/* A hardware device structure exists for each physical device
in the machine */

struct hw_device;

/* A hardware class structure exists for each type of device.
It defines the operations that are allowed on that device.
For example, if there are multiple ROM chips, then there is
a single "ROM" class and multiple ROM device objects. */

struct hw_class
{
	/* Nonzero if the device is readonly */
	int readonly;

	/* Resets the device */
	void (*reset) (struct hw_device *dev);

	/* Reads a byte at a given offset from the beginning of the device. */
	U8 (*read) (struct hw_device *dev, unsigned long phy_addr);

	/* Writes a byte at a given offset from the beginning of the device. */
	void (*write) (struct hw_device *dev, unsigned long phy_addr, U8 val);

	/* Update procedure.  This is called periodically and can be used for
	whatever purpose.  The minimum update interval is once per 1ms.  Leave
	NULL if not required */
	void (*update) (struct hw_device *dev);
};


/* The hardware device structure exists for each instance of a device. */

struct hw_device
{
	/* A pointer to the class object.  This says what kind of device it is. */
	struct hw_class *class_ptr;

	/* The device ID assigned to it.  This is filled in automatically
	by the simulator. */
	unsigned int devid;

	/* The total size of the device in bytes. */
	unsigned long size;

	/* The private pointer, which is interpreted differently for each type
	(hw_class) of device. */
	void *priv;
};

/* The machine structure collects everything about the abstract machine.
The pointer 'machine' points to the machine that is being run. */

extern struct machine *machine;

struct machine
{
	const char *name;
	void (*init) (const char *boot_rom_file);
	void (*fault) (unsigned int addr, unsigned char type);
	void (*dump_thread) (unsigned int thread_id);
	void (*periodic) (void);
	unsigned long cycles_per_sec;
};

struct hw_device *device_attach (struct hw_class *class_ptr, unsigned int size, void *priv);

struct hw_device *ram_create (unsigned long size);
struct hw_device *rom_create (const char *filename, unsigned int maxsize);
struct hw_device *console_create (void);
struct hw_device *disk_create (const char *backing_file, struct hw_device *ram_dev);

#endif /* _M6809_MACHINE_H */
