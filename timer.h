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


#ifndef TIMER_H
#define TIMER_H

/* A hardware timer counts CPU cycles and can generate interrupts periodically. */
struct hwtimer
{
	int count;            /* The current value of the timer */
	unsigned int reload;  /* Value to reload into the timer when it reaches zero */
	unsigned int resolution; /* Resolution of CPU registers (cycles/tick) */
	unsigned int flags;
	unsigned long prev_cycles;
	struct hw_device *int_dev;  /* Which interrupt mux we use */
	unsigned int int_line;  /* Which interrupt to signal */
};

/* The I/O registers exposed by this driver */
#define HWT_COUNT     0  /* The 8-bit timer counter */
#define HWT_RELOAD    1  /* The 8-bit reload counter */
#define HWT_FLAGS     2  /* Misc. flags */
	#define HWTF_INT   0x80   /* Generate interrupt at zero */


/*
 * Called by the system to indicate that some number of CPU cycles have passed.
 */
void hwtimer_decrement (struct hwtimer *timer, unsigned int cycles);
void hwtimer_update (struct hw_device *dev);
U8 hwtimer_read (struct hw_device *dev, unsigned long addr);
void hwtimer_write (struct hw_device *dev, unsigned long addr, U8 val);
void hwtimer_reset (struct hw_device *dev);
void oscillator_reset (struct hw_device *dev);
struct hw_device *hwtimer_create (struct hw_device *int_dev, unsigned int int_line);
struct hw_device *oscillator_create(struct hw_device *int_dev, unsigned int int_line);

#endif
