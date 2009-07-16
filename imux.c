
/* The interrupt multiplexer */

#include "machine.h"
#include "eon.h"

struct imux
{
	unsigned int in_use;    /* Bits for each int that are used */
	unsigned int enabled;   /* Bits for each int that is enabled */
	unsigned int pending;   /* Bits for each int that are active */
	unsigned int src;       /* Source line back to CPU */
};

void imux_refresh (struct imux *mux)
{
	if (mux->pending & mux->enabled)
		request_irq (mux->src);
	else
		release_irq (mux->src);
}


void imux_reset (struct hw_device *dev)
{
	struct imux *mux = (struct imux *)dev->priv;
	mux->enabled = 0;
	mux->pending = 0;
	mux->src = 1;
}

U8 imux_read (struct hw_device *dev, unsigned long addr)
{
	struct imux *mux = (struct imux *)dev->priv;
	switch (addr)
	{
		case IMUX_ADDR - IMUX_ENB:
			/* Return the enable bits */
			return mux->enabled & 0xFF;

		case IMUX_ADDR - IMUX_PEND:
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
		case IMUX_ADDR - IMUX_ENB:
			/* Set the interrupt enables */
			mux->enabled = val;
			break;
		case IMUX_ADDR - IMUX_PEND:
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
	.readonly = 0,
	.reset = imux_reset,
	.read = imux_read,
	.write = imux_write,
};

struct hw_device *imux_create (void)
{
	struct imux *imux = malloc (sizeof (struct imux));
	return device_attach (&imux_class, BUS_MAP_SIZE, imux);
}


