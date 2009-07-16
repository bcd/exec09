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

#ifndef _MACHINE_EON_H
#define _MACHINE_EON_H

/* This file defines the characteristics of the EON machine architecture.
EON is a little more advanced than the 'simple' architecture that runs by
default, and can be used to run more sophiscated programs for the 6809.
However, no actual hardware resembling this exists.

The computer has a total of 1MB of RAM, which is mapped to logical address
0x0000.  As the 6809 only has 64KB addressable at a time, some bank
switching is required.  The address space is defined into 16, 4KB pages.
The pages from 0xE000-0xFFFF are special, though, and allow access to
I/O and a read-only boot ROM.  Thus, pages 0-13 can be mapped to any
of the 1MB RAM, page 14 has all of the I/O registers and some of the boot
ROM, and page 15 has the remaining boot ROM, including interrupt vectors.
The boot ROM file to use is specified as a command-line option to the
emulator.

Each I/O device comprises 128 bytes of address space.  There can be up
to a maximum of 8 devices defined this way.  The "device ID" is just the
offset from the beginning of the I/O region.  At present, 5 devices are
defined:

Device 0 - the MMU.  This provides registers for reprogramming the paging
hardware.

Device 1 - the power manager.  This is not yet implemented.

Device 2 - the console.  This has the basic stdin/stdout, akin to a serial
port.

Device 3 - the display.  Not implemented yet, this would be akin to a
graphical interface.

Device 4 - the disk drive.  See machine.c for a full description of how the
disk works.  EON simulates the disk using a file named 'disk.bin', so its
contents are actually persistent.  Disk transfers conceptually DMA to/from
RAM in 512-byte chunks.  The maximum disk size is 32MB (16-bit sector numbers).

*/

/* RAM */
#define RAM_SIZE 0x100000

/* I/O regions (1KB) */
#define BOOT_IO_ADDR 0xE000
#define DEVICE_BASE(n)  (BOOT_IO_ADDR + (BUS_MAP_SIZE * (n)))

/* The MMU */

	#define MMU_DEVID      0
	#define MMU_ADDR       DEVICE_BASE(MMU_DEVID)
	#define MMU_PAGESIZE   4096
	#define MMU_PAGECOUNT  (MAX_CPU_ADDR / MMU_PAGESIZE)
	#define MMU_PAGEREGS   4

		/* device select */
		#define MMU_DEV(p)      (MMU_ADDR + ((p) * MMU_PAGEREGS) + 0)
		/* 4KB region to map in */
		#define MMU_OFF(p)      (MMU_ADDR + ((p) * MMU_PAGEREGS) + 1)
		/* permissions */
		#define MMU_FLG(p)      (MMU_ADDR + ((p) * MMU_PAGEREGS) + 2)

	#define MMU_FAULT_ADDR (MMU_ADDR + 0x60)
	#define MMU_FAULT_TYPE (MMU_ADDR + 0x62)

/* The Power Manager */

#define POWERMGR_DEVID   1
#define POWERMGR_ADDR    DEVICE_BASE(POWERMGR_DEVID)
#define POWER_CTRL       (POWERMGR_ADDR + 0)
#define POWER_RESET_REG  (POWERMGMT_ADDR + 1)

/* The Console */

	#define CONSOLE_DEVID 2
	#define CONSOLE_ADDR   DEVICE_BASE(CONSOLE_DEVID)
		#define CONSOLE_OUT    (CONSOLE_ADDR + 0)
		#define LEGACY_EXIT    (CONSOLE_ADDR + 1)
		#define CONSOLE_IN     (CONSOLE_ADDR + 2)

#define CON_OUT 0
#define CON_EXIT 1
#define CON_IN 2

/* The Display */

#define DISPLAY_DEVID  3
#define DISPLAY_ADDR   DEVICE_BASE(DISPLAY_DEVID)

/* The Disk Drive */

#define DISK0_DEVID    4
#define DISK_ADDR(n)   DEVICE_BASE(DISK0_DEVID + (n))
#define DISK_SECTOR_COUNT 65536
#define SECTOR_SIZE 512

#define DSK_CTRL 0
	#define DSK_READ 0x1
	#define DSK_WRITE 0x2
	#define DSK_FLUSH 0x4
	#define DSK_ERASE 0x8
#define DSK_ADDR 1
#define DSK_SECTOR 2 /* and 3 */

/* The Interrupt Multiplexer */

#define IMUX_DEVID      5
#define IMUX_ADDR       DEVICE_BASE(IMUX_DEVID)

#define IMUX_ENB    0
#define IMUX_PEND   1

/* Boot ROM region (7KB) */
#define BOOT_ROM_ADDR 0xE400
#define BOOT_ROM_SIZE 0x1C00

#endif /* _MACHINE_EON_H */
