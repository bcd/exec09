
#include "6809.h"
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*command_handler_t) (void);

typedef void (*virtual_handler_t) (unsigned long *val, int writep);

typedef unsigned long absolute_address_t;

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
	int used : 1;
	unsigned int format;
	unsigned int size;
   char expr[128];
} display_t;


#define MAX_BREAKS 32
#define MAX_DISPLAYS 32
#define MAX_HISTORY 10

/**********************************************************/
/********************* Global Data ************************/
/**********************************************************/

unsigned int break_count = 0;
breakpoint_t breaktab[MAX_BREAKS];
unsigned int active_break_count = 0;

unsigned int display_count = 0;
display_t displaytab[MAX_DISPLAYS];

unsigned int history_count = 0;
unsigned long historytab[MAX_HISTORY];

absolute_address_t examine_addr = 0;
unsigned int examine_repeat = 1;
unsigned char examine_format = 'x';
unsigned int examine_size = 1;

unsigned char print_format = 'x';
unsigned int print_size = 1;

char *command_flags;

int exit_command_loop;

unsigned long eval (const char *expr);
extern int auto_break_insn_count;


/**********************************************************/
/******************** 6809 Functions **********************/
/**********************************************************/

void
print_addr (absolute_address_t addr)
{
	printf ("%04X", addr);
}


absolute_address_t
to_absolute (unsigned long cpuaddr)
{
	return cpuaddr;
}

unsigned long
compute_inirq (void)
{
}


unsigned long
compute_infirq (void)
{
}


/**********************************************************/
/*********************** Functions ************************/
/**********************************************************/

void
syntax_error (const char *string)
{
   fprintf (stderr, "error: %s\n", string);
}


void
save_value (unsigned long val)
{
   historytab[history_count++ % MAX_HISTORY] = val;
}


unsigned long
eval_historical (unsigned int id)
{
   return historytab[id % MAX_HISTORY];
}


void
assign_virtual (const char *name, unsigned long val)
{
	virtual_handler_t virtual;
	unsigned long v_val;
	if (sym_find (name, &v_val, 0))
	{
		syntax_error ("???");
		return;
	}

	virtual = (virtual_handler_t)v_val;
	virtual (&val, 1);
}


unsigned long
eval_virtual (const char *name)
{
	virtual_handler_t virtual;
	unsigned long val;

   /* The name of the virtual is looked up in the global
    * symbol table, which holds the pointer to a
    * variable in simulator memory, or to a function
    * that can compute the value on-the-fly. */
	if (sym_find (name, &val, 0))
	{
		syntax_error ("???");
		return 0;
	}

	virtual = (virtual_handler_t)val;
	virtual (&val, 0);
	return val;
}

unsigned long
eval_absolute (const char *page, const char *addr)
{
	unsigned long pageval = eval (page);
	unsigned long cpuaddr = eval (addr);
	/* ... */
}

void
eval_assign (const char *expr, unsigned long val)
{
	if (*expr == '$')
	{
		assign_virtual (expr+1, val);
	}
}


unsigned long
target_read (absolute_address_t addr, unsigned int size)
{
	switch (size)
	{
		case 1:
			return read8 (addr);
		case 2:
			return read16 (addr);
	}
}


void
parse_format_flag (const char *flags, unsigned char *formatp)
{
   while (*flags)
   {
      switch (*flags)
      {
         case 'x':
         case 'd':
         case 'u':
            *formatp = *flags;
            break;
      }
      flags++;
   }
}


void
parse_size_flag (const char *flags, unsigned int *sizep)
{
   while (*flags)
   {
      switch (*flags++)
      {
         case 'b':
            *sizep = 1;
            break;
         case 'w':
            *sizep = 2;
            break;
      }
   }
}


unsigned long
eval_indirect (const char *target_addr, const char *flags)
{
   absolute_address_t addr;
   unsigned int size = 1;

   addr = strtoul (target_addr, NULL, 0);


   return target_read (addr, size);
}


int
fold_binary (const char *expr, const char op, unsigned long *valp)
{
	char *p;
	unsigned long val1, val2;

	if ((p = strchr (expr, op)) == NULL)
		return 0;

   *p++ = '\0';
	val1 = eval (expr);
	val2 = eval (p);

	switch (op)
	{
		case '+': *valp = val1 + val2; break;
		case '-': *valp = val1 - val2; break;
		case '*': *valp = val1 * val2; break;
		case '/': *valp = val1 / val2; break;
	}
	return 1;
}


unsigned long
eval (const char *expr)
{
   char *p;
   unsigned long val;

   if ((p = strchr (expr, '=')) != NULL)
	{
      *p++ = '\0';
		val = eval (p);
		eval_assign (expr, val);
	}
	else if (fold_binary (expr, '+', &val));
	else if (fold_binary (expr, '-', &val));
	else if (fold_binary (expr, '*', &val));
	else if (fold_binary (expr, '/', &val));
   else if (*expr == '$')
   {
      if (expr[1] == '$')
         val = eval_historical (history_count - strtoul (expr+2, NULL, 10));
      else if (isdigit (expr[1]))
         val = eval_historical (strtoul (expr+1, NULL, 10));
      else if (!expr[1])
         val = eval_historical (0);
      else
         val = eval_virtual (expr+1);
   }
   else if ((p = strchr (expr, '?')) != NULL)
   {
      *p++ = '\0';
      val = eval_absolute (expr, p);
   }
   else if (*expr == '*')
   {
      val = eval_indirect (expr+1, "");
   }
   else
   {
      val = strtoul (expr, NULL, 0);
   }

   return val;
}


void brk_enable (breakpoint_t *br, int flag)
{
	if (br->enabled != flag)
	{
		br->enabled = flag;
		if (flag)
			active_break_count++;
		else
			active_break_count--;
	}
}


breakpoint_t *
brkalloc (void)
{
   unsigned int n;
   for (n = 0; n < MAX_BREAKS; n++)
      if (!breaktab[n].used)
      {
         breakpoint_t *br = &breaktab[n];
         br->used = 1;
         br->id = n;
         br->conditional = 0;
         br->threaded = 0;
         brk_enable (br, 1);
         return br;
      }
	return NULL;
}


void
brkfree (breakpoint_t *br)
{
   brk_enable (br, 0);
   br->used = 0;
}


breakpoint_t *
brkfind_by_addr (absolute_address_t addr)
{
   unsigned int n;
   for (n = 0; n < MAX_BREAKS; n++)
		if (breaktab[n].addr == addr)
			return &breaktab[n];
	return NULL;
}

breakpoint_t *
brkfind_by_id (unsigned int id)
{
	return &breaktab[id];
}


void
brkprint (breakpoint_t *brkpt)
{
   if (!brkpt->used)
      return;
   printf ("Breakpoint %d at %08lX", brkpt->id, brkpt->addr);
   if (!brkpt->enabled)
      printf (" (disabled)");
   if (brkpt->conditional)
      printf (" if %s", brkpt->condition);
   if (brkpt->threaded)
      printf (" on thread %d", brkpt->tid);
   putchar ('\n');
}

void
print_value (unsigned long val, const char format, unsigned int size)
{
   char f[8];

	if (format == 'x')
		printf ("0x");
	else if (format == 'o')
		printf ("0");

   sprintf (f, "%%0*%c", format);
   printf (f, size, val);
}


int
print_insn (absolute_address_t addr)
{
	char buf[64];
	int size = dasm (buf, addr);
	printf ("%s", buf);
	return size;
}


void
do_examine (void)
{
   unsigned int n;
	unsigned int objs_per_line = 16;

   if (isdigit (*command_flags))
      examine_repeat = strtoul (command_flags, &command_flags, 0);

	if (*command_flags == 's' || *command_flags == 'i')
		examine_format = *command_flags;
	else
   	parse_format_flag (command_flags, &examine_format);
   parse_size_flag (command_flags, &examine_size);

	if (examine_format == 'i')
		objs_per_line = 1;

   for (n = 0; n < examine_repeat; n++)
   {
		if ((n % objs_per_line) == 0)
		{
			putchar ('\n');
			print_addr (examine_addr);
			printf (": ");
		}

      switch (examine_format)
      {
         case 's': /* string */
            break;

         case 'i': /* instruction */
				examine_addr += print_insn (examine_addr);
            break;

         default:
            print_value (target_read (examine_addr, examine_size),
                         examine_format, examine_size);
            putchar (' ');
      		examine_addr += examine_size;
      }
   }
   putchar ('\n');
}

void
do_print (const char *expr)
{
   unsigned long val = eval (expr);
   printf ("$%d = ", history_count);

   parse_format_flag (command_flags, &print_format);
   parse_size_flag (command_flags, &print_size);
   print_value (val, print_format, print_size);
   putchar ('\n');
   save_value (val);
}

void
do_set (const char *expr)
{
	unsigned long val = eval (expr);
   save_value (val);
}


char *
getarg (void)
{
   return strtok (NULL, " \t\n");
}


/****************** Command Handlers ************************/

void cmd_print (void)
{
   const char *arg = getarg ();
   if (arg)
      do_print (arg);
   else
      do_print ("$");
}


void cmd_set (void)
{
   const char *arg = getarg ();
   if (arg)
      do_set (arg);
   else
      do_set ("$");
}


void cmd_examine (void)
{
   const char *arg = getarg ();
   if (arg)
      examine_addr = eval (arg);
   do_examine ();
}

void cmd_break (void)
{
   const char *arg = getarg ();
   unsigned long val = eval (arg);
   breakpoint_t *br = brkalloc ();
   br->addr = val;
   brkprint (br);
}

void cmd_break_list (void)
{
   unsigned int n;
   for (n = 0; n < MAX_BREAKS; n++)
      brkprint (&breaktab[n]);
}

void cmd_step_next (void)
{
	auto_break_insn_count = 1;
	exit_command_loop = 0;
}

void cmd_continue (void)
{
	exit_command_loop = 0;
}

void cmd_quit (void)
{
	cpu_quit = 0;
	exit_command_loop = 1;
}

void cmd_clear (void)
{
	const char *arg = getarg ();
	unsigned int id = atoi (arg);
	breakpoint_t *br = brkfind_by_id (id);
	if (br->used)
	{
		printf ("Clearing breakpoint %d\n", id);
		brkfree (br);
	}
}


/****************** Parser ************************/

struct command_name
{
   const char *prefix;
   const char *name;
   command_handler_t handler;
} cmdtab[] = {
   { "p", "print", cmd_print },
   { "x", "examine", cmd_examine },
	{ "set", "set", cmd_set },
   { "b", "break", cmd_break },
   { "bl", "blist", cmd_break_list },
   { "cl", "clear", cmd_clear },
   { "s", "step", cmd_step_next },
   { "n", "next", cmd_step_next },
   { "c", "continue", cmd_continue },
	{ "q", "quit", cmd_quit },
	{ "re", "reset", cpu_reset },
#if 0
   { "i", "info", cmd_info },
   { "watch", cmd_watch },
   { "rwatch", cmd_rwatch },
   { "awatch", cmd_awatch },
   { "d", "delete", cmd_delete },
   { "co", "condition", cmd_condition },
   { "tr", "trace", cmd_trace },
   { "di", "disable", cmd_disable },
   { "en", "enable", cmd_enable },
   { "l", "list", cmd_list },
#endif
   { NULL, NULL },
};

command_handler_t
command_lookup (const char *cmd)
{
   struct command_name *cn;
   char *p;

   p = strchr (cmd, '/');
   if (p)
   {
      *p = '\0';
      command_flags = p+1;
   }
   else
      command_flags = "";

   cn = cmdtab;
   while (cn->prefix != NULL)
   {
      if (!strcmp (cmd, cn->prefix))
         return cn->handler;
      if (!strcmp (cmd, cn->name))
         return cn->handler;
      cn++;
   }
   return NULL;
}


void
command_prompt (void)
{
	unsigned int n;
	for (n = 0; n < MAX_DISPLAYS; n++)
	{
		display_t *ds = &displaytab[n];
		if (ds->used)
		{
		}
	}

   fprintf (stderr, "(dbg) ");
   fflush (stderr);
}


void
print_current_insn (void)
{
	absolute_address_t addr = to_absolute (get_pc ());
	print_addr (addr);
	printf (" : ");
	print_insn (addr);
	putchar ('\n');
}


int
command_loop (void)
{
   char buffer[256];
   char prev_buffer[256];
   char *cmd;
   command_handler_t handler;
   int rc;

	exit_command_loop = -1;
   while (exit_command_loop < 0)
   {
		print_current_insn ();
      command_prompt ();
      fgets (buffer, 255, stdin);
      if (feof (stdin))
         break;

      if (buffer[0] == '\n')
         strcpy (buffer, prev_buffer);

      cmd = strtok (buffer, " \t\n");
      if (!cmd)
         continue;
     	strcpy (prev_buffer, cmd);

      handler = command_lookup (cmd);
      if (!handler)
      {
         syntax_error ("no such command");
         continue;
      }

      (*handler) ();
   }
	return (exit_command_loop);
}


void
command_insn_hook (void)
{
	unsigned long abspc;
	breakpoint_t *br;	

	if (active_break_count == 0)
		return;

	abspc = to_absolute (get_pc ());
	br = brkfind_by_addr (abspc);
	if (br && br->enabled)
	{
		printf ("Breakpoint %d reached.\n", br->id);
		monitor_on = 1;
	}
}


void pc_virtual (unsigned long *val, int writep) {
	writep ? set_pc (*val) : (*val = get_pc ());
}
void x_virtual (unsigned long *val, int writep) {
	writep ? set_x (*val) : (*val = get_x ());
}
void y_virtual (unsigned long *val, int writep) {
	writep ? set_y (*val) : (*val = get_y ());
}
void u_virtual (unsigned long *val, int writep) {
	writep ? set_u (*val) : (*val = get_u ());
}
void s_virtual (unsigned long *val, int writep) {
	writep ? set_s (*val) : (*val = get_s ());
}
void d_virtual (unsigned long *val, int writep) {
	writep ? set_d (*val) : (*val = get_d ());
}
void dp_virtual (unsigned long *val, int writep) {
	writep ? set_dp (*val) : (*val = get_dp ());
}
void cc_virtual (unsigned long *val, int writep) {
	writep ? set_cc (*val) : (*val = get_cc ());
}


void cycles_virtual (unsigned long *val, int writep)
{
	if (!writep)
		*val = get_cycles ();
}


void
command_init (void)
{
	sym_add ("pc", pc_virtual, SYM_REGISTER);
	sym_add ("x", x_virtual, SYM_REGISTER);
	sym_add ("y", y_virtual, SYM_REGISTER);
	sym_add ("u", u_virtual, SYM_REGISTER);
	sym_add ("s", s_virtual, SYM_REGISTER);
	sym_add ("d", d_virtual, SYM_REGISTER);
	sym_add ("dp", dp_virtual, SYM_REGISTER);
	sym_add ("cc", cc_virtual, SYM_REGISTER);
	sym_add ("cycles", cycles_virtual, SYM_REGISTER);
}


