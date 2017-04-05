/*
 * Copyright 2009 by Brian Dominy <brian@oddchange.com>
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
#include <stdlib.h>
#include "machine.h"
#include "ioexpand.h"
#include "6809.h"


/* An I/O expander subdivides a single, 128-byte region into 16, 8-byte slots each
of which can hold a device. It is needed to work around the inability of the system
bus to handle attaching devices less than 128 bytes in size.

When mapping a slot, an offset can be applied to the underlying device. An example application is to make the I/O expander occupy fewer than 128 bytes by mapping one or more slots to a large underlying ROM. */

void ioexpand_attach (struct hw_device *expander_dev, int slot, unsigned long offset, struct hw_device *io_dev)
{
	struct ioexpand *iom = (struct ioexpand *)expander_dev->priv;
	iom->ios[slot] = io_dev;
	iom->offset[slot] = offset;
}

void ioexpand_reset (struct hw_device *dev)
{
	int i;
	struct ioexpand *iom = (struct ioexpand *)dev->priv;

	/* Propagate the reset to all devices behind the expander */
	for (i=0; i < NR_IOEXPAND; i++)
		if ((dev = iom->ios[i]) != NULL)
			dev->class_ptr->reset (dev);
}

U8 ioexpand_read (struct hw_device *dev, unsigned long addr)
{
	struct ioexpand *iom = (struct ioexpand *)dev->priv;
        int slot = addr / IO_WINDOW;
	dev = iom->ios[slot];
	if (!dev)
		sim_error ("expander read from %04X has no backing device\n", addr);

        addr = (addr % IO_WINDOW) + iom->offset[slot];
	return dev->class_ptr->read (dev, addr);
}

void ioexpand_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct ioexpand *iom = (struct ioexpand *)dev->priv;
        int slot = addr / IO_WINDOW;
	dev = iom->ios[slot];
	if (!dev)
		sim_error ("expander write %02X to %04X has no backing device\n", val, addr);

        addr = (addr % IO_WINDOW) + iom->offset[slot];
	dev->class_ptr->write (dev, addr, val);
}

struct hw_class ioexpand_class =
{
	.name = "ioexpand",
	.readonly = 0,
	.reset = ioexpand_reset,
	.read = ioexpand_read,
	.write = ioexpand_write,
};

struct hw_device *ioexpand_create (void)
{
	int i;
	struct ioexpand *iom = malloc (sizeof (struct ioexpand));
	for (i=0; i < NR_IOEXPAND; i++) {
		iom->ios[i] = NULL;
		iom->offset[i] = 0;
        }
	return device_attach (&ioexpand_class, BUS_MAP_SIZE, iom);
}
