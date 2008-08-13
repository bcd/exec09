
#include "6809.h"
#include "monitor.h"
#include "machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/errno.h>

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
datatype_t examine_type;

thread_t threadtab[MAX_THREADS];

datatype_t print_type;

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
   printf ("%02X:%04X", addr >> 28, addr & 0xFFFFFF);
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
			return abs_read8 (addr);
		case 2:
			return abs_read8 (addr) << 8 +
            abs_read8 (addr+1);
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


int
fold_binary (const char *expr, const char op, unsigned long *valp)
{
	char *p;
	unsigned long val1, val2;

	if ((p = strchr (expr, op)) == NULL)
		return 0;

   /* If the operator is the first character of the expression,
    * then it's really a unary and shouldn't match here. */
   if (p == expr)
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
eval_mem (const char *expr)
{
   char *p;
   unsigned long val;

   if ((p = strchr (expr, ':')) != NULL)
   {
      *p++ = '\0';
      val = eval (expr) * 0x10000000L + eval (p);
   }
   else
   {
      val = to_absolute (eval (expr));
   }
   return val;
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
   else if (*expr == '*')
   {
      absolute_address_t addr = eval_mem (expr+1);
      return target_read (addr, 1);
   }
   else if (*expr == '@')
   {
      val = eval_mem (expr+1);
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

   if (brkpt->on_execute)
      printf ("Breakpoint");
   else
   {
      printf ("Watchpoint");
      if (brkpt->on_read)
         printf ("(%s)", brkpt->on_write ? "RW" : "RO");
   }

   printf (" %d at ", brkpt->id);
   print_addr (brkpt->addr);
   if (!brkpt->enabled)
      printf (" (disabled)");
   if (brkpt->conditional)
      printf (" if %s", brkpt->condition);
   if (brkpt->threaded)
      printf (" on thread %d", brkpt->tid);
   putchar ('\n');
}


display_t *
display_alloc (void)
{
}


void
display_free (display_t *ds)
{
}


void
display_print (void)
{
}


void
print_value (unsigned long val, datatype_t *typep)
{
   char f[8];

	if (typep->format == 'x')
		printf ("0x");
	else if (typep->format == 'o')
		printf ("0");

   sprintf (f, "%%0*%c", typep->format);
   printf (f, typep->size, val);
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
		examine_type.format = *command_flags;
	else
      parse_format_flag (command_flags, &examine_type.format);
   parse_size_flag (command_flags, &examine_type.size);

	if (examine_type.format == 'i')
		objs_per_line = 1;

   for (n = 0; n < examine_repeat; n++)
   {
		if ((n % objs_per_line) == 0)
		{
			putchar ('\n');
			print_addr (examine_addr);
			printf (": ");
		}

      switch (examine_type.format)
      {
         case 's': /* string */
            break;

         case 'i': /* instruction */
				examine_addr += print_insn (examine_addr);
            break;

         default:
            print_value (target_read (examine_addr, examine_type.size),
                         &examine_type);
            putchar (' ');
      		examine_addr += examine_type.size;
      }
   }
   putchar ('\n');
}

void
do_print (const char *expr)
{
   unsigned long val = eval (expr);
   printf ("$%d = ", history_count);

   parse_format_flag (command_flags, &print_type.format);
   parse_size_flag (command_flags, &print_type.size);
   print_value (val, &print_type);
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
      examine_addr = eval_mem (arg);
   do_examine ();
}

void cmd_break (void)
{
   const char *arg = getarg ();
   unsigned long val = eval_mem (arg);
   breakpoint_t *br = brkalloc ();
   br->addr = val;
   br->on_execute = 1;
   brkprint (br);
}


void cmd_watch1 (int on_read, int on_write)
{
   const char *arg = getarg ();
   absolute_address_t addr = eval_mem (arg);
   breakpoint_t *br = brkalloc ();
   br->addr = addr;
   br->on_read = on_read;
   br->on_write = on_write;
   brkprint (br);
}


void cmd_watch (void)
{
   cmd_watch1 (0, 1);
}


void cmd_rwatch (void)
{
   cmd_watch1 (1, 0);
}

void cmd_awatch (void)
{
   cmd_watch1 (1, 1);
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

void cmd_delete (void)
{
   const char *arg = getarg ();
   unsigned int id = atoi (arg);
   breakpoint_t *br = brkfind_by_id (id);
   if (br->used)
   {
      printf ("Deleting breakpoint %d\n", id);
      brkfree (br);
   }
}

/****************** Parser ************************/

void cmd_help (void);

struct command_name
{
   const char *prefix;
   const char *name;
   command_handler_t handler;
   const char *help;
} cmdtab[] = {
   { "p", "print", cmd_print,
      "Print the value of an expression" },
   { "set", "set", cmd_set,
      "Set an internal variable/target memory" },
   { "x", "examine", cmd_examine,
      "Examine raw memory" },
   { "b", "break", cmd_break,
      "Set a breakpoint" },
   { "bl", "blist", cmd_break_list,
      "List all breakpoints" },
   { "d", "delete", cmd_delete,
      "Delete a breakpoint" },
   { "s", "step", cmd_step_next,
      "Step to the next instruction" },
   { "n", "next", cmd_step_next,
      "Continue up to the next instruction" },
   { "c", "continue", cmd_continue,
      "Continue the program" },
   { "q", "quit", cmd_quit,
      "Quit the simulator" },
   { "re", "reset", cpu_reset,
      "Reset the CPU" },
   { "h", "help", cmd_help,
      "Display this help" },
   { "wa", "watch", cmd_watch },
   { "rwa", "rwatch", cmd_rwatch },
   { "awa", "awatch", cmd_awatch },
   { "?", "?", cmd_help },
#if 0
   { "cl", "clear", cmd_clear },
   { "i", "info", cmd_info },
   { "co", "condition", cmd_condition },
   { "tr", "trace", cmd_trace },
   { "di", "disable", cmd_disable },
   { "en", "enable", cmd_enable },
   { "l", "list", cmd_list },
#endif
   { NULL, NULL },
};

void cmd_help (void)
{
   struct command_name *cn = cmdtab;
   while (cn->prefix != NULL)
   {
      if (cn->help)
         printf ("%s (%s) - %s\n",
            cn->name, cn->prefix, cn->help);
      cn++;
   }
}

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

      do {
         errno = 0;
         fgets (buffer, 255, stdin);
      } while (errno != 0);

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
   absolute_address_t abspc;
	breakpoint_t *br;

	if (active_break_count == 0)
		return;

	abspc = to_absolute (get_pc ());
	br = brkfind_by_addr (abspc);
	if (br && br->enabled && br->on_execute)
	{
		printf ("Breakpoint %d reached.\n", br->id);
		monitor_on = 1;
	}
}


void
command_read_hook (absolute_address_t addr)
{
	breakpoint_t *br = brkfind_by_addr (addr);
	if (br && br->enabled && br->on_read)
   {
      printf ("Watchpoint %d triggered.\n", br->id);
      monitor_on = 1;
   }
}


void
command_write_hook (absolute_address_t addr)
{
	breakpoint_t *br = brkfind_by_addr (addr);
	if (br && br->enabled && br->on_write)
   {
      printf ("Watchpoint %d triggered.\n", br->id);
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
command_periodic (int signo)
{
   if (!monitor_on && signo == SIGALRM)
   {
      /* TODO */
   }
}


void
command_init (void)
{
   struct itimerval itimer;
   struct sigaction sact;
   int rc;

   sym_add ("pc", (unsigned long)pc_virtual, SYM_REGISTER);
   sym_add ("x", (unsigned long)x_virtual, SYM_REGISTER);
   sym_add ("y", (unsigned long)y_virtual, SYM_REGISTER);
   sym_add ("u", (unsigned long)u_virtual, SYM_REGISTER);
   sym_add ("s", (unsigned long)s_virtual, SYM_REGISTER);
   sym_add ("d", (unsigned long)d_virtual, SYM_REGISTER);
   sym_add ("dp", (unsigned long)dp_virtual, SYM_REGISTER);
   sym_add ("cc", (unsigned long)cc_virtual, SYM_REGISTER);
   sym_add ("cycles", (unsigned long)cycles_virtual, SYM_REGISTER);

   examine_type.format = 'x';
   examine_type.size = 1;

   print_type.format = 'x';
   print_type.size = 1;

   sigemptyset (&sact.sa_mask);
   sact.sa_flags = 0;
   sact.sa_handler = command_periodic;
   sigaction (SIGALRM, &sact, NULL);

   itimer.it_interval.tv_sec = 1;
   itimer.it_interval.tv_usec = 0;
   itimer.it_value.tv_sec = 1;
   itimer.it_value.tv_usec = 0;
   rc = setitimer (ITIMER_REAL, &itimer, NULL);
   if (rc < 0)
      fprintf (stderr, "couldn't register interval timer\n");
}

/* vim: set ts=3: */
/* vim: set expandtab: */
