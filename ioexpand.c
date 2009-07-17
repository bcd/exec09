
#include <stdio.h>
#include <stdlib.h>
#include "machine.h"

/* An I/O expander allows a single, 128-byte region to be shared among multiple
devices.  It subdivides the region into 16 8-byte subdevices.   It is needed
to work around the inability of the system bus to handle attaching devices
less than 128 bytes wide. */

#define NR_IOEXPAND 16
#define IO_WINDOW 8

struct ioexpand
{
	struct hw_device *ios[NR_IOEXPAND];
};


void ioexpand_attach (struct hw_device *expander_dev, int slot, struct hw_device *io_dev)
{
	struct ioexpand *iom = (struct ioexpand *)expander_dev->priv;
	iom->ios[slot] = io_dev;
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
	dev = iom->ios[addr / IO_WINDOW];
	if (!dev)
		sim_error ("expander read from %04X has no backing device\n", addr);
	return dev->class_ptr->read (dev, addr % IO_WINDOW);
}

void ioexpand_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct ioexpand *iom = (struct ioexpand *)dev->priv;
	dev = iom->ios[addr / IO_WINDOW];
	if (!dev)
		sim_error ("expander write %02X to %04X has no backing device\n", val, addr);
	dev->class_ptr->write (dev, addr % IO_WINDOW, val);
}

struct hw_class ioexpand_class =
{
	.readonly = 0,
	.reset = ioexpand_reset,
	.read = ioexpand_read,
	.write = ioexpand_write,
};

struct hw_device *ioexpand_create (void)
{
	int i;
	struct ioexpand *iom = malloc (sizeof (struct ioexpand));
	for (i=0; i < NR_IOEXPAND; i++)
		iom->ios[i] = NULL;
	return device_attach (&ioexpand_class, BUS_MAP_SIZE, iom);
}
