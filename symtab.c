
#include "6809.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


struct stringspace *current_stringspace;

struct symbol *symtab[MAX_SYMBOL_HASH];


struct stringspace *stringspace_create (void)
{
	struct stringspace *ss = malloc (sizeof (struct stringspace));
	ss->used = 0;
	return ss;
}

const char *stringspace_copy (const char *string)
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


unsigned int sym_hash (const char *name)
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


int sym_find (const char *name, unsigned long *value, unsigned int type)
{
	unsigned int hash = sym_hash (name);
	struct symbol *chain = symtab[hash];
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
	return -1;
}


void sym_add (const char *name, unsigned long value, unsigned int type)
{
	unsigned int hash = sym_hash (name);
	struct symbol *chain = symtab[hash];
	struct symbol *s = malloc (sizeof (struct symbol));
	s->name = stringspace_copy (name);
	s->value = value;
	s->type = type;
	s->chain = chain;
	symtab[hash] = s;
}


void sym_init (void)
{
	current_stringspace = stringspace_create ();
	memset (symtab, 0, sizeof (symtab));
}

