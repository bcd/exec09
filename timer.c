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
#include "6809.h"
#include "imux.h"
#include "timer.h"

/* A hardware timer counts CPU cycles and can generate interrupts periodically. */

/*
 * Called by the system to indicate that some number of CPU cycles have passed.
 */
void hwtimer_decrement (struct hwtimer *timer, unsigned int cycles)
{
	/* If either the counter or the reload register is nonzero, the timer
	is considered running.  Otherwise, nothing to do */
	if (!timer->count && !timer->reload)
		return;

	/* Decrement the counter.  Is it zero/negative? */
	timer->count -= cycles;
	if (timer->count <= 0)
	{
		/* If interrupt is configured and enabled, generate one now */
		if (timer->int_dev && timer->flags & HWTF_INT)
		{
			imux_assert (timer->int_dev, timer->int_line);
		}

		/* If it is negative, we need to make it positive again.
		If reload is nonzero, add that, to simulate the timer "wrapping".
		Otherwise, fix it at zero. */
		if (timer->count < 0)
		{
			if (timer->reload > 0)
			{
				timer->count += timer->reload;
				/* Note: if timer->count is still negative, the reload value
				is lower than the frequency at which the system is updating the
				timers, and we would need to simulate two interrupts here
				perhaps.  For later. */
				if (timer->count < 0)
					sim_error ("timer count = %d, reload = %d\n", timer->count, timer->reload);
			}
			else
			{
				timer->count = 0;
			}
		}
	}
}

void hwtimer_update (struct hw_device *dev)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;
	unsigned long cycles = get_cycles ();
	hwtimer_decrement (timer, cycles - timer->prev_cycles);
	timer->prev_cycles = cycles;
}


U8 hwtimer_read (struct hw_device *dev, unsigned long addr)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;
	switch (addr)
	{
		case HWT_COUNT:
			return timer->count / timer->resolution;
		case HWT_RELOAD:
			return timer->reload / timer->resolution;
		case HWT_FLAGS:
			return timer->flags;
                default:
                        fprintf(stderr, "Read hwtimer from undefined addr\n");
                        return 0x42;
	}
}

void hwtimer_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;
	switch (addr)
	{
		case HWT_COUNT:
			timer->count = val * timer->resolution;
			break;
		case HWT_RELOAD:
			timer->reload = val * timer->resolution;
			break;
		case HWT_FLAGS:
			timer->flags = val;
                        break;
	}
}

void hwtimer_reset (struct hw_device *dev)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;
	timer->count = 0;
	timer->flags = 0;
	timer->resolution = 128;
	timer->prev_cycles = get_cycles ();
}

void oscillator_reset (struct hw_device *dev)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;
	hwtimer_reset (dev);
	timer->count = timer->reload;
	if (timer->int_dev)
		timer->flags |= HWTF_INT;
}

struct hw_class hwtimer_class =
{
	.name = "hwtimer",
	.readonly = 0,
	.reset = hwtimer_reset,
	.read = hwtimer_read,
	.write = hwtimer_write,
	.update = hwtimer_update,
};

struct hw_device *hwtimer_create (struct hw_device *int_dev, unsigned int int_line)
{
	struct hwtimer *timer = malloc (sizeof (struct hwtimer));
	timer->reload = 0;
	timer->int_dev = int_dev;
	timer->int_line = int_line;
	return device_attach (&hwtimer_class, 16, timer); /* 16 = sizeof I/O window */
}

struct hw_class oscillator_class =
{
	.readonly = 0,
	.reset = oscillator_reset,
	.read = NULL,
	.write = NULL,
	.update = hwtimer_update,
};

struct hw_device *oscillator_create(struct hw_device *int_dev, unsigned int int_line)
{
	struct hwtimer *timer = malloc (sizeof (struct hwtimer));
	timer->reload = 2048; /* cycles per pulse */
	timer->int_dev = int_dev;
	timer->int_line = int_line;
	return device_attach (&oscillator_class, 0, timer);
}
