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

#include <stdio.h>
#include "machine.h"
#include "eon.h"

/* The disk drive is emulated as follows:
 * The disk is capable of "bus-mastering" and is able to dump data directly
 * into a RAM device, without CPU-involvement.  (The pages do not even need to
 * be mapped.)  A transaction is initiated with the following parameters:
 *
 * - address of RAM, aligned to 512 bytes, must reside in lower 32KB.
 *   Thus there are 64 possible sector locations.
 * - address of disk sector, given as a 16-bit value.  This allows for up to
 *   a 32MB disk.
 * - direction, either to disk or from disk.
 *
 * Emulation is synchronous with respect to the CPU.  TODO: this should use
 * interrupts and add real some latency.
 */

struct disk_geometry
{
	unsigned int sectors_per_track;
	unsigned int tracks_per_cylinder;
};

struct disk_priv
{
	FILE *fp;
	struct hw_device *dev;
	unsigned long offset;
	struct hw_device *ramdev;
	unsigned int sectors;
	char *ram;

	unsigned int cycles_to_irq;
	struct hw_device *int_dev;
	unsigned int int_line;
};


void disk_update (struct hw_device *dev)
{
	/* Simulate the way that an actual disk would work.
	Induce some latency into the process. */
}

U8 disk_read (struct hw_device *dev, unsigned long addr)
{
	struct disk_priv *disk = (struct disk_priv *)dev->priv;
}

void disk_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct disk_priv *disk = (struct disk_priv *)dev->priv;

	switch (addr)
	{
		case DSK_ADDR:
			disk->ram = disk->ramdev->priv + val * SECTOR_SIZE;
			break;
		case DSK_SECTOR:
			disk->offset = val; /* high byte */
			break;
		case DSK_SECTOR+1: /* low byte */
			/* Note, only writes to the low byte cause an update. */
			disk->offset = (disk->offset << 8) | val;
			disk->offset *= SECTOR_SIZE;
			fseek (disk->fp, disk->offset, SEEK_SET);
			break;
		case DSK_CTRL:
			if (val & DSK_READ)
			{
				fread (disk->ram, SECTOR_SIZE, 1, disk->fp);
			}
			else if (val & DSK_WRITE)
			{
				fwrite (disk->ram, SECTOR_SIZE, 1, disk->fp);
			}
			else if (val & DSK_ERASE)
			{
				char empty_sector[SECTOR_SIZE];
				memset (empty_sector, 0xff, SECTOR_SIZE);
				fwrite (empty_sector, SECTOR_SIZE, 1, disk->fp);
			}

			if (val & DSK_FLUSH)
			{
				fflush (disk->fp);
			}
			break;
	}
}

void disk_reset (struct hw_device *dev)
{
	//struct disk_priv *disk = (struct disk_priv *)dev->priv;
	disk_write (dev, DSK_ADDR, 0);
	disk_write (dev, DSK_SECTOR, 0);
	disk_write (dev, DSK_SECTOR+1, 0);
	disk_write (dev, DSK_CTRL, DSK_FLUSH);
}

void disk_format (struct hw_device *dev)
{
	unsigned int sector;
	struct disk_priv *disk = (struct disk_priv *)dev->priv;

	for (sector = 0; sector < disk->sectors; sector++)
	{
		disk_write (dev, DSK_SECTOR, sector >> 8);
		disk_write (dev, DSK_SECTOR+1, sector & 0xFF);
		disk_write (dev, DSK_CTRL, DSK_ERASE);
	}
	disk_write (dev, DSK_CTRL, DSK_FLUSH);
}

struct hw_class disk_class =
{
	.readonly = 0,
	.reset = disk_reset,
	.read = disk_read,
	.write = disk_write,
	.update = disk_update,
};

struct hw_device *disk_create (const char *backing_file,
	struct hw_device *ram_dev)
{
	struct disk_priv *disk = malloc (sizeof (struct disk_priv));
	int newdisk = 0;

	disk->fp = file_open (NULL, backing_file, "r+b");
	if (disk->fp == NULL)
	{
		printf ("warning: disk does not exist, creating\n");
		disk->fp = file_open (NULL, backing_file, "w+b");
		newdisk = 1;
		if (disk->fp == NULL)
		{
			printf ("warning: disk not created\n");
		}
	}

	disk->ram = 0;
	disk->ramdev = ram_dev;
	disk->dev = device_attach (&disk_class, 4, disk);
	disk->sectors = DISK_SECTOR_COUNT;
	disk->cycles_to_irq = 0;

	if (newdisk)
		disk_format (disk->dev);

	return disk->dev;
}

