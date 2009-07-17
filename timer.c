
#include <stdio.h>
#include "machine.h"

/* A hardware timer counts CPU cycles and can generate interrupts periodically. */
struct hwtimer
{
	int count;            /* The current value of the timer */
	unsigned int reload;  /* Value to reload into the timer when it reaches zero */
	unsigned int resolution; /* Resolution of CPU registers (cycles/tick) */
	unsigned int flags;
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
void hwtimer_decrement (struct hw_device *dev, unsigned int cycles)
{
	struct hwtimer *timer = (struct hwtimer *)dev->priv;

	/* If either the counter or the reload register is nonzero, the timer
	is considered running.  Otherwise, nothing to do */
	if (!timer->count && !timer->reload)
		return;

	/* Decrement the counter.  Is it zero/negative? */
	timer->count -= cycles;
	if (timer->count <= 0)
	{
		/* If interrupt is enabled, generate that now */
		if (timer->flags & HWTF_INT)
		{
		}

		/* If it is negative, we need to make it positive again.
		If reload is nonzero, add that, to simulate the timer "wrapping".
		Otherwise, fix it at zero. */
		if (timer->flags < 0)
		{
			if (timer->reload)
			{
				timer->count += timer->reload;
				/* Note: if timer->count is still negative, the reload value
				is lower than the frequency at which the system is updating the
				timers, and we would need to simulate two interrupts here
				perhaps.  For later. */
			}
			else
			{
				timer->count = 0;
			}
		}
	}
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
	timer->reload = 0;
	timer->resolution = 128;
}

struct hw_class hwtimer_class =
{
	.readonly = 0,
	.reset = hwtimer_reset,
	.read = hwtimer_read,
	.write = hwtimer_write,
};

struct hw_device *hwtimer_create (void)
{
	struct hwtimer *timer = malloc (sizeof (struct hwtimer));
	return device_attach (&hwtimer_class, 16, timer); /* 16 = sizeof I/O window */
}


