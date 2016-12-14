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

/* The interrupt multiplexer */

#ifndef IMUX_H
#define IMUX_H

struct imux
{
	unsigned int in_use;    /* Bits for each int that are used */
	unsigned int enabled;   /* Bits for each int that is enabled */
	unsigned int pending;   /* Bits for each int that are active */
	unsigned int src;       /* Source line back to CPU */
};


/*
 * Refresh the state of the interrupt line back to the CPU.
 * src == 1 refers to IRQ, src == 2 for FIRQ.
 */
void imux_refresh (struct imux *mux);
void imux_reset (struct hw_device *dev);
U8 imux_read (struct hw_device *dev, unsigned long addr);
void imux_write (struct hw_device *dev, unsigned long addr, U8 val);

/*
 * Register an interrupt line with the multiplexer.
 * This just tracks which ones are in use.
 */
void imux_register (struct hw_device *dev, unsigned int sig);

/*
 * Assert an edge-triggered interrupt line.
 */
void imux_assert (struct hw_device *dev, unsigned int sig);

struct hw_device *imux_create (unsigned int cpu_line);

#endif
