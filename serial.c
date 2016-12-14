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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "machine.h"
#include "serial.h"

/* Emulate a serial port.  Basically this driver can be used for any byte-at-a-time
input/output interface. */

void serial_update (struct serial_port *port)
{
	fd_set infds, outfds;
	struct timeval timeout;

	FD_ZERO (&infds);
	FD_SET (port->fin, &infds);
	FD_ZERO (&outfds);
	FD_SET (port->fout, &outfds);
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	select (2, &infds, &outfds, NULL, &timeout);
	if (FD_ISSET (port->fin, &infds))
		port->status |= SER_STAT_READOK;
	else
		port->status &= ~SER_STAT_READOK;
	if (FD_ISSET (port->fout, &outfds))
		port->status |= SER_STAT_WRITEOK;
	else
		port->status &= ~SER_STAT_WRITEOK;
}

U8 serial_read (struct hw_device *dev, unsigned long addr)
{
	struct serial_port *port = (struct serial_port *)dev->priv;
	int retval;
	serial_update (port);
	switch (addr)
	{
		case SER_DATA:
		{
			U8 val;
			if (!(port->status & SER_STAT_READOK))
				return 0xFF;
			retval = read (port->fin, &val, 1);
			assert(retval != -1);
			return val;
		}
		case SER_CTL_STATUS:
                        return port->status;
                default:
                        fprintf(stderr, "serial_read() from undefined addr\n");
        }
        return 0x42;
}

void serial_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct serial_port *port = (struct serial_port *)dev->priv;
	int retval;
	switch (addr)
	{
		case SER_DATA:
		{
			U8 v = val;
			retval = write (port->fout, &v, 1);
			assert(retval != -1);
			break;
		}
		case SER_CTL_STATUS:
			port->ctrl = val;
			break;
	}
}

void serial_reset (struct hw_device *dev)
{
	struct serial_port *port = (struct serial_port *)dev->priv;
	port->ctrl = 0;
	port->status = 0;
}

struct hw_class serial_class =
{
	.name = "serial",
	.readonly = 0,
	.reset = serial_reset,
	.read = serial_read,
	.write = serial_write,
};

extern U8 null_read (struct hw_device *dev, unsigned long addr);

struct hw_device* serial_create (void)
{
	struct serial_port *port = malloc (sizeof (struct serial_port));
	port->fin = STDIN_FILENO;
	port->fout = STDOUT_FILENO;
	return device_attach (&serial_class, 4, port);
}

struct hw_device* hostfile_create (const char *filename, int flags)
{
	struct serial_port *port = malloc (sizeof (struct serial_port));
	port->fin = port->fout = open(filename, O_CREAT | flags, S_IRUSR | S_IWUSR);
	return device_attach (&serial_class, 4, port);
}
