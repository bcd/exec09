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


#ifndef M6809_H
#define M6809_H

#include "config.h"
#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#else
#error
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#else
#error
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#else
#error
#endif

typedef uint8_t UINT8;
typedef signed char INT8;

typedef uint16_t UINT16;
typedef signed short INT16;

typedef uint32_t UINT32;
typedef signed int INT32;

typedef uint16_t target_addr_t;

#include "machine.h"

#define E_FLAG 0x80
#define F_FLAG 0x40
#define H_FLAG 0x20
#define I_FLAG 0x10
#define N_FLAG 0x08
#define Z_FLAG 0x04
#define V_FLAG 0x02
#define C_FLAG 0x01

extern int debug_enabled;
extern int need_flush;
extern unsigned int total;
extern int dump_cycles_on_success;

#ifdef OLDSYS
extern UINT8 *memory;
#endif

/* Primitive read/write macros */
#define read8(addr)        cpu_read8 (addr)
#define write8(addr,val)   do { cpu_write8 (addr, val); } while (0)

/* 16-bit versions */
#define read16(addr)       (read8(addr) << 8 | read8(addr+1))
#define write16(addr,val)  do { write8(addr, val & 0xFF); write8(addr+1, (val >> 8) & 0xFF) } while (0)

/* Fetch macros */

#define fetch8()           read8(pc++)
#define fetch16()          (pc += 2, read16(pc-2))

/* 6809.c */
extern int cpu_quit;
extern int cpu_execute (int);
extern void cpu_reset (void);

extern unsigned get_a  (void);
extern unsigned get_b  (void);
extern unsigned get_cc (void);
extern unsigned get_dp (void);
extern unsigned get_x  (void);
extern unsigned get_y  (void);
extern unsigned get_s  (void);
extern unsigned get_u  (void);
extern unsigned get_pc (void);
extern unsigned get_d  (void);
extern void set_a  (unsigned);
extern void set_b  (unsigned);
extern void set_cc (unsigned);
extern void set_dp (unsigned);
extern void set_x  (unsigned);
extern void set_y  (unsigned);
extern void set_s  (unsigned);
extern void set_u  (unsigned);
extern void set_pc (unsigned);
extern void set_d  (unsigned);

/* monitor.c */
extern int monitor_on;
extern int check_break (unsigned);
extern void monitor_init (void); 
extern int monitor6809 (void);
extern int dasm (char *, int);

extern int load_hex (char *);
extern int load_s19 (char *);
extern int load_bin (char *,int);

#define MAX_STRINGSPACE 32000
#define MAX_SYMBOL_HASH 1009

#define SYM_KEYWORD 0
#define SYM_COMMAND 1
#define SYM_REGISTER 2
#define SYM_MEM 3
#define SYM_INT 4

/* symtab.c */
struct stringspace
{
	char space[32000];
	unsigned int used;
};


struct symbol
{
	char *name;
	unsigned long value;
	unsigned int type;
	struct symbol *chain;
};


struct symtab
{
   struct symbol *syms_by_name[MAX_SYMBOL_HASH];
   struct symbol *syms_by_value[MAX_SYMBOL_HASH];
   struct symtab *parent;
};

void sym_add (const char *name, unsigned long value, unsigned int type);
int sym_find (const char *name, unsigned long *value, unsigned int type);
const char *sym_lookup (struct symtab *symtab, unsigned long value);

typedef void (*command_handler_t) (void);

typedef void (*virtual_handler_t) (unsigned long *val, int writep);

typedef unsigned int thread_id_t;

typedef struct
{
   int id : 8;
   int used : 1;
   int enabled : 1;
   int conditional : 1;
   int threaded : 1;
   int on_read : 1;
   int on_write : 1;
   int on_execute : 1;
   int size : 4;
   absolute_address_t addr;
   char condition[128];
   thread_id_t tid;
   unsigned int pass_count;
   unsigned int ignore_count;
} breakpoint_t;


typedef struct
{
   unsigned char format;
   unsigned int size;
} datatype_t;


typedef struct
{
   int used : 1;
   datatype_t type;
   char expr[128];
} display_t;


typedef struct
{
   int id : 8;
   thread_id_t tid;
} thread_t;


#define MAX_BREAKS 32
#define MAX_DISPLAYS 32
#define MAX_HISTORY 10
#define MAX_THREADS 64


#endif /* M6809_H */
