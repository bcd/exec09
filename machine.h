/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
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

typedef unsigned char U8;
typedef unsigned short U16;

#define MAX_CPU_ADDR 65536

/* The generic bus architecture. */

/* Up to 32 devices may be connected.  Each device is addressed by a 32-bit physical address */
#define MAX_BUS_DEVICES 32

/* A bus map says how to convert a CPU address into a bus address.
A single bus map defines 128 bytes of address space; for a 64KB CPU,
that requires a total of 512 such structures. */
#define BUS_MAP_SIZE 128

#define MAP_ANY 0x0
#define MAP_READONLY 0x1

#define FAULT_NONE 0
#define FAULT_NOT_WRITABLE 1
#define FAULT_NO_RESPONSE 2


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

struct hw_class
{
	void (*reset) (struct hw_device *dev);
	U8 (*read) (struct hw_device *dev, unsigned long phy_addr);
	void (*write) (struct hw_device *dev, unsigned long phy_addr, U8 val);
};

struct hw_device
{
	struct hw_class *class_ptr;
	unsigned int devid;
	unsigned long size;
	void *priv;
};

/* Predefine address regions, needed at boot time */

/* I/O regions (1KB) */
#define BOOT_IO_ADDR 0xE000
#define DEVICE_BASE(n)  (BOOT_IO_ADDR + (BUS_MAP_SIZE * (n)))

	#define MMU_DEVID      0
	#define MMU_ADDR       DEVICE_BASE(MMU_DEVID)
		#define MMU_DEV(p)      (MMU_ADDR + (p * 4) + 0) /* device select */
		#define MMU_OFF(p)      (MMU_ADDR + (p * 4) + 1) /* 8KB region to map in */
		#define MMU_FLG(p)      (MMU_ADDR + (p * 4) + 2) /* permissions */

	#define MMU_FAULT_ADDR (MMU_ADDR + 0x60)
	#define MMU_FAULT_TYPE (MMU_ADDR + 0x62)

	#define POWERMGR_DEVID 1
	#define POWERMGR_ADDR  DEVICE_BASE(POWERMGR_DEVID)
		#define POWER_CTRL      (POWERMGR_ADDR + 0)

	#define CONSOLE_DEVID 2
	#define CONSOLE_ADDR   DEVICE_BASE(CONSOLE_DEVID)
		#define CONSOLE_OUT    (CONSOLE_ADDR + 0)
		#define LEGACY_EXIT    (CONSOLE_ADDR + 1)
		#define CONSOLE_IN     (CONSOLE_ADDR + 2)

	#define DISPLAY_ADDR   DEVICE_BASE(3)

	#define DISK_ADDR(n)   DEVICE_BASE(4 + (n))

/* Boot ROM region (7KB) */
#define BOOT_ROM_ADDR 0xE400

/* Define TARGET_MACHINE to the correct machine_config structure */
#ifdef CONFIG_WPC
#define TARGET_MACHINE_NAME "WPC"
#define TARGET_INIT wpc_init
#define TARGET_READ wpc_read
#define TARGET_WRITE wpc_write
#endif

#ifndef TARGET_MACHINE
#define TARGET_MACHINE_NAME "SIMPLE"
#define TARGET_INIT simple_init
#define TARGET_READ simple_read
#define TARGET_WRITE simple_write
#endif

#endif /* M6809_MACHINE_H */
