
#include <stdio.h>
#include <string.h>
#include "machine.h"

#define MISSING 0xff
#define mmu_device (device_table[0])

unsigned int device_count = 0;
struct hw_device *device_table[MAX_BUS_DEVICES];

struct bus_map busmaps[NUM_BUS_MAPS];

U16 fault_addr;

U8 fault_type;


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
		map->flags = flags;
		count--;
		map++;
		offset += BUS_MAP_SIZE;
	}
}

void page_fault (unsigned int addr, unsigned char type)
{
	fault_addr = addr;
	fault_type = type;
	irq ();
}


static struct bus_map *find_map (unsigned int addr)
{
	return &busmaps[addr / BUS_MAP_SIZE];
}

static struct hw_device *find_device (unsigned int addr, unsigned char id)
{
	if (id == 0xFF)
		page_fault (addr, FAULT_NO_RESPONSE);
	return device_table[id];
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
	struct hw_device *dev = find_device (addr, map->devid);
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
	struct hw_device *dev = find_device (addr, map->devid);
	struct hw_class *class_ptr = dev->class_ptr;
	unsigned long phy_addr = map->offset + addr % BUS_MAP_SIZE;

	if (map->flags & MAP_READONLY)
	{
		page_fault (addr, FAULT_NOT_WRITABLE);
	}

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
	bus_map (0x0000, dev->devid, 0, 0x10000, MAP_ANY);
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
		bus_map (BOOT_ROM_ADDR, dev->devid, 0, 0x2000, MAP_READONLY);
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
	bus_map (CONSOLE_ADDR, dev->devid, 0, BUS_MAP_SIZE, MAP_ANY);

	/* For legacy code */
	bus_map (0xff00, dev->devid, 0, BUS_MAP_SIZE, MAP_ANY);
}

/**********************************************************/

#define PAGE_SIZE 8192
#define PAGE_COUNT 8
#define PAGE_REGS 8

U8 mmu_regs[PAGE_COUNT][PAGE_REGS];

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
			unsigned int page = (addr / PAGE_REGS) % PAGE_COUNT;
			unsigned int reg = addr % PAGE_REGS;
			return mmu_regs[page][reg];
		}
	}
}

void mmu_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	unsigned int page = (addr / PAGE_REGS) % PAGE_COUNT;
	unsigned int reg = addr % PAGE_REGS;
	mmu_regs[page][reg] = val;
	bus_map (page * PAGE_SIZE, mmu_regs[page][0], mmu_regs[page][1] * PAGE_SIZE, PAGE_SIZE, MAP_ANY);
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
	bus_map (MMU_ADDR, dev->devid, 0, BUS_MAP_SIZE, MAP_ANY);
}

/**********************************************************/

/* The disk drive is emulated as follows:
 * The disk is capable of "bus-mastering" and is able to dump data directly
 * into the RAM, without CPU-involvement.  (The pages do not even need to
 * be mapped.)  A transaction is initiated with the following parameters:
 *
 * - address of RAM, aligned to 512 bytes, must reside in lower 32KB.
 *   Thus there are 64 possible sector locations.
 * - address of disk sector, given as a 16-bit value.  This allows for up to
 *   a 32MB disk.
 * - direction, either to disk or from disk.
 *
 * Emulation is synchronous with respect to the CPU.
 */

#define SECTOR_SIZE 512

#define DSK_CTRL 0
	#define DSK_READ 0x1
	#define DSK_WRITE 0x2
	#define DSK_FLUSH 0x4
	#define DSK_ERASE 0x8
#define DSK_ADDR 1
#define DSK_SECTOR 2 /* and 3 */

struct disk_priv
{
	FILE *fp;
	struct hw_device *dev;
	unsigned long offset;
	struct hw_device *ramdev;
	unsigned int sectors;
	char *ram;
};


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
		case DSK_SECTOR+1:
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
	struct disk_priv *disk = (struct disk_priv *)dev->priv;
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
	.reset = disk_reset,
	.read = disk_read,
	.write = disk_write,
};

void disk_init (const char *backing_file)
{
	struct disk_priv *disk = malloc (sizeof (struct disk_priv));
	int newdisk = 0;

	disk->fp = fopen (backing_file, "r+b");
	if (disk->fp == NULL)
	{
		printf ("warning: disk does not exist, creating\n");
		disk->fp = fopen (backing_file, "w+b");
		newdisk = 1;
		if (disk->fp == NULL)
		{
			printf ("warning: disk not created\n");
		}
	}

	disk->ram = 0;
	disk->ramdev = device_table[1];
	disk->dev = device_attach (&disk_class, BUS_MAP_SIZE, disk);
	disk->sectors = 65536;

	bus_map (DISK_ADDR(0), disk->dev->devid, 0, BUS_MAP_SIZE, MAP_ANY);

	if (newdisk)
	{
		disk_format (disk->dev);
	}
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
	disk_init ("disk.bin");

	//dump_machine ();
}

