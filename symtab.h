/*
 * Copyright 2008 by Brian Dominy <brian@oddchange.com>
 *
 * This file is part of the Portable 6809 Simulator.
 *
 * The Simulator is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Simulator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef SYMTAB_H
#define SYMTAB_H

#include "6809.h"

void sym_init (void);
void symtab_print (struct symtab *symtab);

#endif
