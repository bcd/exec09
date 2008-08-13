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

struct symtab default_symtab;

struct symtab *current_symtab = &default_symtab;


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


int sym_find (const char *name, unsigned long *value, unsigned int type)
{
	unsigned int hash = sym_hash_name (name);
   struct symtab *symtab = current_symtab;

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
   				return -1;
   			*value = chain->value;
   			return 0;
   		}
   		chain = chain->chain;
   	}
      symtab = symtab->parent;
   }

	return -1;
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
   		chain = chain->chain;
   	}
      symtab = symtab->parent;
   }
	return NULL;
}


void sym_add (const char *name, unsigned long value, unsigned int type)
{
	unsigned int hash;
	struct symbol *s, *chain;
   
   s = malloc (sizeof (struct symbol));
	s->name = stringspace_copy (name);
	s->value = value;
	s->type = type;
   
   hash = sym_hash_name (name);
	chain = current_symtab->syms_by_name[hash];
	s->chain = chain;
	current_symtab->syms_by_name[hash] = s;

   hash = sym_hash_value (value);
	chain = current_symtab->syms_by_value[hash];
	s->chain = chain;
	current_symtab->syms_by_value[hash] = s;
}


void sym_init (void)
{
	current_stringspace = stringspace_create ();
	memset (&default_symtab, 0, sizeof (default_symtab));
}

