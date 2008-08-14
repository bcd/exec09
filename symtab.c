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

#include "6809.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct stringspace *current_stringspace;

struct symtab program_symtab;

struct symtab internal_symtab;

struct symtab auto_symtab;

struct stringspace *stringspace_create (void)
{
	struct stringspace *ss = malloc (sizeof (struct stringspace));
	ss->used = 0;
	return ss;
}

char *stringspace_copy (const char *string)
{
	unsigned int len = strlen (string) + 1;
	char *result;

	if (current_stringspace->used + len > MAX_STRINGSPACE)
		current_stringspace = stringspace_create ();

	result = current_stringspace->space + current_stringspace->used;
	strcpy (result, string);
	current_stringspace->used += len;
	return result;
}


unsigned int sym_hash_name (const char *name)
{
	unsigned int hash = *name & 0x1F;
	if (*name++ != '\0')
	{
		hash = (hash << 11) + (544 * *name);
		if (*name++ != '\0')
		{
		hash = (hash << 5) + (17 * *name);
		}
	}
	return hash % MAX_SYMBOL_HASH;
}


unsigned int sym_hash_value (unsigned long value)
{
   return value % MAX_SYMBOL_HASH;
}


/**
 * Lookup the symbol 'name'.
 * Returns 0 if the symbol exists (and optionally stores its value
 * in *value if not NULL), or -1 if it does not exist.
 */
struct symbol *sym_find1 (struct symtab *symtab,
                          const char *name, unsigned long *value,
								  unsigned int type)
{
	unsigned int hash = sym_hash_name (name);

   /* Search starting in the current symbol table, and if that fails,
    * try its parent tables. */
   while (symtab != NULL)
   {
      /* Find the list of elements that hashed to this string. */
	   struct symbol *chain = symtab->syms_by_name[hash];

      /* Scan the list for an exact match, and return it. */
   	while (chain != NULL)
   	{
   		if (!strcmp (name, chain->name))
   		{
   			if (type && (chain->type != type))
   				return NULL;
            if (value)
					*value = chain->value;
   			return chain;
   		}
   		chain = chain->name_chain;
   	}
      symtab = symtab->parent;
   }

	return NULL;
}


int sym_find (struct symtab *symtab,
              const char *name, unsigned long *value, unsigned int type)
{
	return sym_find1 (symtab, name, value, type) ? 0 : -1;
}


const char *sym_lookup (struct symtab *symtab, unsigned long value)
{
	unsigned int hash = sym_hash_value (value);

   while (symtab != NULL)
   {
	   struct symbol *chain = symtab->syms_by_value[hash];
   	while (chain != NULL)
   	{
   		if (value == chain->value)
   			return chain->name;
   		chain = chain->value_chain;
   	}
      symtab = symtab->parent;
   }
	return NULL;
}


void sym_add (struct symtab *symtab,
              const char *name, unsigned long value, unsigned int type)
{
	unsigned int hash;
	struct symbol *s, *chain;
   
   s = malloc (sizeof (struct symbol));
	s->name = stringspace_copy (name);
	s->value = value;
	s->type = type;
   
   hash = sym_hash_name (name);
	chain = symtab->syms_by_name[hash];
	s->name_chain = chain;
	symtab->syms_by_name[hash] = s;

   hash = sym_hash_value (value);
	chain = symtab->syms_by_value[hash];
	s->value_chain = chain;
	symtab->syms_by_value[hash] = s;
}


void sym_set (struct symtab *symtab,
              const char *name, unsigned long value, unsigned int type)
{
	struct symbol * s = sym_find1 (symtab, name, NULL, type);
	if (s)
		s->value = value;
	else
		sym_add (symtab, name, value, type);
}


void symtab_init (struct symtab *symtab)
{
	memset (symtab, 0, sizeof (struct symtab));
}


void symtab_reset (struct symtab *symtab)
{
	/* TODO */
	symtab_init (symtab);
}


void sym_init (void)
{
	current_stringspace = stringspace_create ();

	symtab_init (&program_symtab);
	symtab_init (&internal_symtab);
	symtab_init (&auto_symtab);
}

