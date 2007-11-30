/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
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

/* Define TARGET_MACHINE to the correct machine_config structure */
#ifdef CONFIG_WPC
#define TARGET_MACHINE_NAME "WPC"
#define TARGET_INIT wpc_init
#define TARGET_READ wpc_read
#define TARGET_WRITE wpc_write
#endif

#ifndef TARGET_MACHINE
#define TARGET_MACHINE_NAME "SIMPLE"
#define TARGET_INIT simple_init
#define TARGET_READ simple_read
#define TARGET_WRITE simple_write
#endif

#endif /* M6809_MACHINE_H */
