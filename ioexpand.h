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

#ifndef IOEXPAND_H
#define IOEXPAND_H

/* An I/O expander subdivides a single, 128-byte region into 16, 8-byte slots
   each of which can hold a device. It is needed to work around the inability
   of the system bus to handle attaching devices less than 128 bytes in size.

   When mapping a slot, an offset can be applied to the underlying device. An
   example application is to make the I/O expander occupy fewer than 128 bytes
   by mapping one or more slots to a large underlying ROM.
 */


#define NR_IOEXPAND 16
#define IO_WINDOW 8

struct ioexpand
{
	struct hw_device *ios[NR_IOEXPAND];
	unsigned long offset[NR_IOEXPAND];
};

struct hw_device *ioexpand_create (void);
void ioexpand_attach (struct hw_device *expander_dev, int slot, unsigned long offset, struct hw_device *io_dev);
void ioexpand_reset (struct hw_device *dev);
U8 ioexpand_read (struct hw_device *dev, unsigned long addr);
void ioexpand_write (struct hw_device *dev, unsigned long addr, U8 val);
struct hw_device *ioexpand_create (void);

#endif
