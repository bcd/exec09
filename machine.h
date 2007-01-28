/*
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


#ifndef M6809_MACHINE_H
#define M6809_MACHINE_H

#ifdef CONFIG_WPC
#define TARGET_MACHINE wpc_machine
#endif

#ifndef TARGET_MACHINE
#define TARGET_MACHINE simple_machine
#endif

struct machine_config {
	const char *name;
	void (*init) (void);
	int (*read_byte) (target_addr_t addr, uint8_t *val);
	int (*write_byte) (target_addr_t addr, uint8_t val);
};


extern struct machine_config TARGET_MACHINE;

#endif /* M6809_MACHINE_H */
