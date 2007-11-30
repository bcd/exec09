/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
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

#include <sys/select.h>
#include "6809.h"
#include "monitor.h"
#include "simple-mmap.h"

// #define static_inline static inline
#define static_inline

/**************** Console driver ***************************/

static_inline int console_read_ready (void)
{
	fd_set fds;
	struct timeval timeout;
	uint8_t c;

	FD_ZERO (&fds);
	FD_SET (0, &fds);
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	if (select (1, &fds, NULL, NULL, &timeout))
		return 1;
	return 0;
}

static_inline uint8_t console_read (void)
{
	return getchar ();
}

static_inline void console_write (uint8_t val)
{
	putchar (val);
	fflush (stdout);
}

/**************** Virtual disk driver ***************************/

const char *vdisk_filename = "simple.vd";

unsigned int vdisk_enables[VDISK_NUM_REGIONS] = { 0, };

unsigned int vdisk_offsets[VDISK_NUM_REGIONS];

uint8_t vdisk_buffers[VDISK_NUM_REGIONS][VDISK_REGION_SIZE];

static_inline void vdisk_swapout (unsigned int region)
{
	FILE *fp = fopen (vdisk_filename, "a+b");
	if (!fp)
	{
		fprintf (stderr, "cannot open for disk for writing\n");
		return;
	}

	if (fseek (fp, vdisk_offsets[region], SEEK_SET) == -1)
	{
		fprintf (stderr, "disk read seek error\n");
		return;
	}

	fwrite (vdisk_buffers[region], sizeof (unsigned char), VDISK_REGION_SIZE, fp);
	fclose (fp);
}


static_inline void vdisk_swapin (unsigned int region)
{
	FILE *fp = fopen (vdisk_filename, "a+b");
	if (!fp)
	{
		fprintf (stderr, "cannot open for disk for reading\n");
		return;
	}

	if (fseek (fp, vdisk_offsets[region], SEEK_SET) == -1)
	{
		fprintf (stderr, "disk read seek error\n");
		return;
	}

	fread (vdisk_buffers[region], sizeof (unsigned char), VDISK_REGION_SIZE, fp);
	fclose (fp);
}

static_inline void vdisk_map_high (unsigned int region, unsigned int value)
{
	vdisk_offsets[region] = (vdisk_offsets[region] & (0xFF00 * VDISK_REGION_SIZE))
		| ((value << 8) * VDISK_REGION_SIZE);
}

static_inline void vdisk_map_low (unsigned int region, unsigned int value)
{
	if (vdisk_enables[region])
		vdisk_swapout (region);
	vdisk_offsets[region] = 
		(vdisk_offsets[region] & (0x00FF * VDISK_REGION_SIZE))
		| (value * VDISK_REGION_SIZE);
	vdisk_swapin (region);
	vdisk_enables[region] = 1;
}

static_inline int vdisk_addr_match (unsigned int region, target_addr_t addr)
{
	target_addr_t region_addr = VDISK_REGION0 + region * VDISK_REGION_SIZE;
	if ((addr >= region_addr)
		&& (addr < region_addr + VDISK_REGION_SIZE)
		&& vdisk_enables[region])
		return 1;
	else
		return 0;
}

static_inline uint8_t vdisk_read (unsigned int region, target_addr_t addr)
{
	return vdisk_buffers[region][addr & 0x1FFF];
}


static_inline void vdisk_write (unsigned int region, target_addr_t addr, uint8_t val)
{
	unsigned int offset = vdisk_offsets[region];
	vdisk_buffers[region][addr & 0x1FFF] = val;
}


/**************** Target functions ***************************/

uint8_t simple_read (target_addr_t addr)
{
	switch (addr)
	{
		case SIMPLE_CONSOLE_READ: return console_read ();
		default: 
			if (vdisk_addr_match (0, addr)) return vdisk_read (0, addr);
			else if (vdisk_addr_match (1, addr)) return vdisk_read (1, addr);
			else if (vdisk_addr_match (2, addr)) return vdisk_read (2, addr);
			else
				return read8 (addr);
	}
}


void
simple_write (target_addr_t addr, uint8_t val)
{
	switch (addr)
	{
		case SIMPLE_CONSOLE_WRITE: console_write (val); break;
		case SIMPLE_SYSTEM_EXIT: sim_exit (val); break;
		case VDISK_ADDR0: vdisk_map_high (0, val); break;
		case VDISK_ADDR0+1: vdisk_map_low (0, val); break;
		case VDISK_ADDR1: vdisk_map_high (1, val); break;
		case VDISK_ADDR1+1: vdisk_map_low (1, val); break;
		case VDISK_ADDR2: vdisk_map_high (2, val); break;
		case VDISK_ADDR2+1: vdisk_map_low (2, val); break;
		default: 
			if (vdisk_addr_match (0, addr)) vdisk_write (0, addr, val);
			else if (vdisk_addr_match (1, addr)) vdisk_write (1, addr, val);
			else if (vdisk_addr_match (2, addr)) vdisk_write (2, addr, val);
			else
				write8 (addr, val);
			break;
	}
}


void
simple_init (void)
{
	add_named_symbol ("CONSOLE_OUT", 0xFF00, NULL);
	add_named_symbol ("EXIT", 0xFF01, NULL);
	add_named_symbol ("CONSOLE_IN", 0xFF02, NULL);
}

