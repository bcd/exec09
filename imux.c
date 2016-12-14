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

#include <stdlib.h>
#include "machine.h"
#include "eon.h"
#include "6809.h"
#include "imux.h"


/*
 * Refresh the state of the interrupt line back to the CPU.
 * src == 1 refers to IRQ, src == 2 for FIRQ.
 */
void imux_refresh (struct imux *mux)
{
	if (mux->pending & mux->enabled)
	{
		if (mux->src == 1)
			request_irq (mux->src);
		else
			request_firq (mux->src);
	}
	else
	{
		if (mux->src == 1)
			release_irq (mux->src);
		else
			release_firq (mux->src);
	}
}

void imux_reset (struct hw_device *dev)
{
	struct imux *mux = (struct imux *)dev->priv;
	mux->enabled = 0;
	mux->pending = 0;
}

U8 imux_read (struct hw_device *dev, unsigned long addr)
{
	struct imux *mux = (struct imux *)dev->priv;
	switch (addr)
	{
		case IMUX_ENB:
			/* Return the enable bits */
			return mux->enabled & 0xFF;

		case IMUX_PEND:
			/* Return the pending bits */
			return mux->pending & 0xFF;
	}
	return -1;
}

void imux_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct imux *mux = (struct imux *)dev->priv;
	switch (addr)
	{
		case IMUX_ENB:
			/* Set the interrupt enables */
			mux->enabled = val;
			break;
		case IMUX_PEND:
			/* Clear pending interrupt source */
			mux->pending &= ~val;
			break;
	}
	imux_refresh (mux);
}

/*
 * Register an interrupt line with the multiplexer.
 * This just tracks which ones are in use.
 */
void imux_register (struct hw_device *dev, unsigned int sig)
{
	struct imux *mux = (struct imux *)dev->priv;
	mux->in_use |= (1 << sig);
}

/*
 * Assert an edge-triggered interrupt line.
 */
void imux_assert (struct hw_device *dev, unsigned int sig)
{
	struct imux *mux = (struct imux *)dev->priv;
	mux->pending |= (1 << sig);
	imux_refresh (mux);
}

struct hw_class imux_class =
{
	.name = "imux",
	.readonly = 0,
	.reset = imux_reset,
	.read = imux_read,
	.write = imux_write,
};

struct hw_device *imux_create (unsigned int cpu_line)
{
	struct imux *imux = malloc (sizeof (struct imux));
	imux->src = cpu_line;
	return device_attach (&imux_class, BUS_MAP_SIZE, imux);
}
