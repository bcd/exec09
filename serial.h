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

#ifndef SERIAL_H
#define SERIAL_H

/* Emulate a serial port.  Basically this driver can be used for any byte-at-a-time
input/output interface. */
struct serial_port
{
	unsigned int ctrl;
	unsigned int status;
	int fin;
	int fout;
};

/* The I/O registers exposed by this driver */
#define SER_DATA         0   /* Data input/output */
#define SER_CTL_STATUS   1   /* Control (write) and status (read) */
	#define SER_CTL_ASYNC   0x1   /* Enable async mode (more realistic) */
	#define SER_CTL_RESET   0x2   /* Reset device */

	#define SER_STAT_READOK  0x1
	#define SER_STAT_WRITEOK 0x2

void serial_update (struct serial_port *port);
U8 serial_read (struct hw_device *dev, unsigned long addr);
void serial_write (struct hw_device *dev, unsigned long addr, U8 val);
void serial_reset (struct hw_device *dev);
extern U8 null_read (struct hw_device *dev, unsigned long addr);
struct hw_device* serial_create (void);
struct hw_device* hostfile_create (const char *filename, int flags);

#endif
