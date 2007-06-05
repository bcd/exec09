/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
 * Copyright 2006 by Brian Dominy <brian@oddchange.com>
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

#include "6809.h"
#include "monitor.h"


int
simple_read_byte (target_addr_t addr, uint8_t *val)
{
	switch (addr)
		{
		case 0xE002:
			*val = getchar ();
			return 0;

		default:
			return -1;
		}
}


int
simple_write_byte (target_addr_t addr, uint8_t val)
{
	switch (addr)
		{
		case 0xFF00:
			putchar (val);
			need_flush = 1;
			return 0;

		case 0xFF01:
			sim_exit (val);
			return 0;

		default:
			return -1;
		}
}


void
simple_init (void)
{
	add_named_symbol ("REG_OUT", 0xFF00, NULL);
	add_named_symbol ("REG_EXIT", 0xFF01, NULL);
	add_named_symbol ("REG_IN", 0xFF02, NULL);
}


struct machine_config simple_machine = {
	.name = "simple",
	.init = simple_init,
	.read_byte = simple_read_byte,
	.write_byte = simple_write_byte,
};


