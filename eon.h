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
		#define MMU_DEV(p)      (MMU_ADDR + (p * MMU_PAGEREGS) + 0)
		/* 4KB region to map in */
		#define MMU_OFF(p)      (MMU_ADDR + (p * MMU_PAGEREGS) + 1)
		/* permissions */
		#define MMU_FLG(p)      (MMU_ADDR + (p * MMU_PAGEREGS) + 2)

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

/* Boot ROM region (7KB) */
#define BOOT_ROM_ADDR 0xE400
#define BOOT_ROM_SIZE 0x1C00

#endif /* _MACHINE_EON_H */
