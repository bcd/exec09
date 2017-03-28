/* -*- c-file-style: "ellemtel"  -*- */

#include "6809.h"
#include "monitor.h"
#include "machine.h"
#include "symtab.h"
#include <sys/errno.h>
#include <unistd.h>
#include <ctype.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
#error
#endif
#ifdef HAVE_READLINE
# include <stdio.h>
# include <readline/readline.h>
# include <readline/history.h>
#endif

struct termios old_tio, new_tio;

typedef struct
{
   unsigned int size;
   unsigned int count;
   char **strings;
} cmdqueue_t;


typedef enum
{
   LVALUE,
   RVALUE,
} eval_mode_t;

#define MAKE_ADDR(devno, phyaddr) ((devno * 0x10000000L) + phyaddr)

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

/* Thread tracking. thread_current points to a location in
 * target memory where the current thread ID is kept.  thread_id
 * is the debugger's current cached value of that, to avoid
 * reading memory constantly.  The size allows for targets to
 * define the ID format differently. */
unsigned int thread_id_size = 2;
absolute_address_t thread_current;
absolute_address_t thread_id = 0;
thread_t threadtab[MAX_THREADS];

#define MAX_CMD_QUEUES 8
int command_stack_depth = -1;
cmdqueue_t command_stack[MAX_CMD_QUEUES];

#define MAX_TRACE 256
target_addr_t trace_buffer[MAX_TRACE];
unsigned int trace_offset = 0;

int stop_after_ms = 0;

datatype_t print_type;

char *command_flags;

int exit_command_loop;

#define IRQ_CYCLE_COUNTS 128
unsigned int irq_cycle_tab[IRQ_CYCLE_COUNTS] = { 0, };
unsigned int irq_cycle_entry = 0;
unsigned long irq_cycles = 0;

unsigned long eval (char *expr, char *eflag);
unsigned long eval_mem (char *expr, eval_mode_t mode, char *eflag);
static int print_insn_long (absolute_address_t addr);
extern int auto_break_insn_count;

FILE *command_input;

/**********************************************************/
/******************** 6809 Functions **********************/
/**********************************************************/

void print_addr (absolute_address_t addr)
{
   const char *name;

   print_device_name (addr >> 28);
   putchar (':');
   printf ("0x%04lX", addr & 0xFFFFFF);

   name = sym_lookup (&program_symtab, addr);
   if (name)
      printf ("  %-18.18s", name);
   else
      printf ("%-20.20s", "");
}


/**********************************************************/
/*********************** Functions ************************/
/**********************************************************/

void syntax_error (const char *string)
{
   fprintf (stderr, "error: %s\n", string);
}

void report_errors (const char eflag)
{
   if (eflag &    1) fprintf (stderr, "error: bad operator in expression\n");
   if (eflag &    2) fprintf (stderr, "error: bad lvalue in expression\n");
   if (eflag &    4) fprintf (stderr, "error: bad rvalue in expression\n");
   if (eflag &    8) fprintf (stderr, "error: non-existent symbol in expression\n");
   if (eflag & 0x10) fprintf (stderr, "error: non-existent $symbol in expression\n");
   if (eflag & 0x20) fprintf (stderr, "error: bad numeric literal\n");
   if (eflag & 0x40) fprintf (stderr, "error: unrecognised $symbol in assignment\n");
   if (eflag & 0x80) fprintf (stderr, "error: missing assignment\n");
}

void save_value (unsigned long val)
{
   historytab[history_count++ % MAX_HISTORY] = val;
}

unsigned long eval_historical (unsigned int id)
{
   return historytab[id % MAX_HISTORY];
}

void assign_virtual (const char *name, unsigned long val, char *eflag)
{
   unsigned long v_val;

   if (!sym_find (&auto_symtab, name, &v_val, 0))
   {
      virtual_handler_t virtual = (virtual_handler_t)v_val;
      virtual (&val, 1);
      return;
   }
   else if (!strcmp (name, "thread_current"))
   {
      printf ("Thread pointer initialized to ");
      print_addr (val);
      putchar ('\n');
      thread_current = val;
   }
   else
   {
      *eflag = *eflag | 0x40; /* not found */
   }
}

unsigned long eval_virtual (const char *name, char *eflag)
{
   unsigned long val;

   /* The name of the virtual is looked up in the auto
    * symbol table, which holds a function that can
    * compute the value on-the-fly. If not found there
    * a value of 0 is returned along with an error flag.
    */
   if (!sym_find (&auto_symtab, name, &val, 0))
   {
      virtual_handler_t virtual = (virtual_handler_t)val;
      virtual (&val, 0);
   }
   else
   {
      *eflag = *eflag | 0x10;
      val = 0;
   }

   return val;
}

void eval_assign (char *expr, unsigned long val, char *eflag)
{
   if (*expr == '$')
   {
      assign_virtual (expr+1, val, eflag);
   }
   else
   {
      absolute_address_t dst = eval_mem(expr, LVALUE, eflag);

      if (!*eflag)
         abs_write8(dst, (U8) val);
   }
}

unsigned long target_read (absolute_address_t addr, unsigned int size)
{
   if (size == 1)
      return abs_read8(addr);
   else
      return abs_read16(addr);
}

/* Extract any valid format flags - ignore anything else
 * allows format and size flags to be mingled but provides no way
 * to detect and report unrecognised flags)
 */
void parse_format_flag (const char *flags, unsigned char *formatp)
{
   while (*flags)
   {
      switch (*flags)
      {
         case 'X':
         case 'x':
         case 'd':
         case 'u':
         case 'o':
         case 'a':
         case 's':
         case 'c':
            *formatp = *flags;
            break;
      }
      flags++;
   }
}

/* Extract any valid size flags - ignore anything else
 * (allows format and size flags to be mingled but provides no way
 * to detect and report unrecognised flags)
 */
void parse_size_flag (const char *flags, unsigned int *sizep)
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

char* match_binary (char *expr, const char *op, char **secondp)
{
   char *p;
   p = strstr (expr, op);
   if (!p)
      return NULL;
   *p = '\0';
   p += strlen (op);
   *secondp = p;
   return expr;
}

int fold_comparisons (char *expr, unsigned long *value, char *eflag)
{
   char *p;
   if (match_binary (expr, "==", &p))
      *value = (eval (expr, eflag) == eval (p, eflag));
   else if (match_binary (expr, "!=", &p))
      *value = (eval (expr, eflag) != eval (p, eflag));
   else
      return 0;

   return 1;
}

int fold_binary (char *expr, const char op, unsigned long *valp, char *eflag)
{
   char *p;
   unsigned long val1, val2;

   if ((p = strchr (expr, op)) == NULL)
      return 0;

   /* If the operator is the first character of the expression,
    * then it's really a unary and shouldn't match here.
    */
   if (p == expr)
      return 0;

   *p++ = '\0';
   val1 = eval (expr, eflag);
   val2 = eval (p, eflag);

   switch (op)
   {
      case '+': *valp = val1 + val2; break;
      case '-': *valp = val1 - val2; break;
      case '*': *valp = val1 * val2; break;
      case '/': *valp = val1 / val2; break;
   }
   return 1;
}

/*
 * Evaluate a memory expression, as an lvalue or rvalue.
 */
unsigned long eval_mem (char *expr, eval_mode_t mode, char *eflag)
{
   char *p;
   unsigned long val;

   /* First evaluate the address */
   if ((p = strchr (expr, ':')) != NULL)
   {
      *p++ = '\0';
      val = MAKE_ADDR (eval (expr, eflag), eval (p, eflag));
   }
   else if (isalpha (*expr) || (*expr == '_'))
   {
      if (!sym_find (&program_symtab, expr, &val, 0));
      else if (!sym_find (&internal_symtab, expr, &val, 0));
      else
      {
         val = 0;
         *eflag = *eflag | 8;
      }
   }
   else
   {
      val = to_absolute (eval (expr, eflag));
   }

   /* If mode is RVALUE, then dereference it */
   if (mode == RVALUE)
      val = target_read (val, 1);

   return val;
}

/*
 * Evaluate an expression, given as a string.
 * The return is the value (rvalue) of the expression.
 *
 * TODO:
 * - Support typecasts ( {TYPE}ADDR )
 *
 */
unsigned long eval(char *expr, char *eflag)
{
   char *p;
   unsigned long val;

   if (fold_comparisons (expr, &val, eflag));
   else if ((p = strchr (expr, '=')) != NULL)
   {
      /* Assignment. Change = to 0 to break the string in two.
       * Remainder of eval is the LHS, p is the RHS
       */
      *p++ = '\0';
      val = eval (p, eflag); /* Evaluate RHS */
      eval_assign (expr, val, eflag); /* Evaluate LHS and assign RHS value */
   }
   else if (fold_binary (expr, '+', &val, eflag));
   else if (fold_binary (expr, '-', &val, eflag));
   else if (fold_binary (expr, '*', &val, eflag));
   else if (fold_binary (expr, '/', &val, eflag));
   else if (*expr == '$')
   {
      if (expr[1] == '$') /* $$n */
         val = eval_historical (history_count-1 - strtoul (expr+2, NULL, 10));
      else if (isdigit (expr[1])) /* $n */
         val = eval_historical (strtoul (expr+1, NULL, 10));
      else if (!expr[1]) /* $ */
         val = eval_historical (history_count-1);
      else /* variable from one of the symbol tables */
         val = eval_virtual (expr+1, eflag);
   }
   /* For a symbol 'fred' 'print fred' and 'set fred=4'
    * treat fred as an RVALUE so they read and write memory
    * at the address associated with the value of fred.
    * 'print &fred' and 'set &fred=4' display and change
    * the value of the symbol fred.
    */
   else if (*expr == '&')
   {
      val = eval_mem (expr+1, LVALUE, eflag);
   }
   else if (isalpha (*expr) || (*expr == '_'))
   {
      val = eval_mem (expr, RVALUE, eflag);
   }
   /* Try to interpet it as a numeric literal */
   else
   {
      val = strtoul (expr, &p, 0);
      if (expr==p)
      {
         *eflag = *eflag | 0x20;
      }
   }

   return val;
}

void brk_enable(breakpoint_t *br, int flag)
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

breakpoint_t* brkalloc (void)
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
         br->keep_running = 0;
         br->ignore_count = 0;
         br->temp = 0;
         br->on_execute = 0;
         brk_enable (br, 1);
         return br;
      }
   return NULL;
}

void brkfree (breakpoint_t *br)
{
   brk_enable (br, 0);
   br->used = 0;
}

void brkfree_temps (void)
{
   unsigned int n;
   for (n = 0; n < MAX_BREAKS; n++)
      if (breaktab[n].used && breaktab[n].temp)
      {
         brkfree (&breaktab[n]);
      }
}

breakpoint_t* brkfind_by_addr (absolute_address_t addr)
{
   unsigned int n;
   for (n = 0; n < MAX_BREAKS; n++)
      if (breaktab[n].addr == addr)
         return &breaktab[n];
   return NULL;
}

breakpoint_t* brkfind_by_id (unsigned int id)
{
   return &breaktab[id];
}

void brkprint (breakpoint_t *brkpt)
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
   if (brkpt->keep_running)
      printf (", print-only");
   if (brkpt->temp)
      printf (", temp");
   if (brkpt->ignore_count)
      printf (", ignore %d times\n", brkpt->ignore_count);
   if (brkpt->write_mask)
      printf (", mask 0x%02X\n", brkpt->write_mask);
   putchar ('\n');
}

display_t* display_alloc (void)
{
   unsigned int n;
   for (n = 0; n < MAX_DISPLAYS; n++)
   {
      display_t *ds = &displaytab[n];
      if (!ds->used)
      {
         ds->used = 1;
         return ds;
      }
   }
   return NULL;
}

void display_free (display_t *ds)
{
}

void print_value (unsigned long val, datatype_t *typep)
{
   char f[8];

   switch (typep->format)
   {
      case 'a':
         print_addr (val);
         return;

      case 'c':
      {
         char c;
         c = val;
         if ((c < 32) | (c > 126)) c = '.';
         putchar(c);
         return;
      }

      case 's':
      {
         absolute_address_t addr = (absolute_address_t)val;
         char c;

         putchar ('"');
         while ((c = abs_read8 (addr++)) != '\0')
            putchar (c);
         putchar ('"');
         return;
      }

      case 't':
         /* TODO : print as binary integer */
         break;
   }

   if ((typep->format == 'x') | (typep->format == 'X'))
   {
      printf ("0x");
      sprintf (f, "%%0%d%c", typep->size * 2, typep->format);
   }
   else if (typep->format == 'o')
   {
      printf ("0");
      sprintf (f, "%%%c", typep->format);
   }
   else
      sprintf (f, "%%%c", typep->format);

   printf (f, val);
}

void display_print (void)
{
   char eflag = 0;
   unsigned int n;
   char comma = '\0';

   for (n = 0; n < MAX_DISPLAYS; n++)
   {
      display_t *ds = &displaytab[n];
      if (ds->used)
      {
         char expr[256];
         strcpy (expr, ds->expr);
         printf ("%c %s = ", comma, expr);
         print_value (eval (expr, &eflag), &ds->type);
         comma = ',';
      }
   }

   if (comma)
      putchar ('\n');
   if (eflag)
      report_errors(eflag);
}

int print_insn (absolute_address_t addr)
{
   char buf[64];
   int size = dasm (buf, addr);
   printf ("%s", buf);
   return size;
}

void do_examine (void)
{
   unsigned int n;
   unsigned int objs_per_line = 16;

   if (isdigit (*command_flags))
      examine_repeat = strtoul (command_flags, &command_flags, 0);

   if (*command_flags == 'i')
      examine_type.format = *command_flags;
   else
      parse_format_flag (command_flags, &examine_type.format);

   parse_size_flag (command_flags, &examine_type.size);

   switch (examine_type.format)
   {
      case 'i':
         objs_per_line = 1;
         break;

      case 'w':
         objs_per_line = 8;
         break;

      case 'c':
         objs_per_line = 32;
         break;
   }

   for (n = 0; n < examine_repeat; n++)
   {
      if ((n % objs_per_line) == 0)
      {
         if (n > 0) putchar ('\n');
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
            if (examine_type.format != 'c') putchar (' ');
            examine_addr += examine_type.size;
      }
   }
   putchar ('\n');
}

void do_print (char *expr)
{
   char eflag = 0;
   unsigned long val = eval (expr, &eflag);

   parse_format_flag (command_flags, &print_type.format);
   parse_size_flag (command_flags, &print_type.size);

   if (eflag)
      report_errors(eflag);
   else
   {
      printf ("$%d = ", history_count);
      print_value (val, &print_type);
      putchar ('\n');
      save_value (val);
   }
}

void do_set (char *expr)
{
   char eflag = 0;

   (void)eval (expr, &eflag);
   /* too late to prevent an assignment, but at least report problems */
   if (eflag)
      report_errors(eflag);
}

/* TODO - WPC */
#define THREAD_DATA_PC 3
#define THREAD_DATA_ROMBANK 9

void print_thread_data (absolute_address_t th)
{
   U8 b;
   U16 w;
   absolute_address_t pc;

   w = abs_read16 (th + THREAD_DATA_PC);
   b = abs_read8 (th + THREAD_DATA_ROMBANK);
   if (w >= 0x8000)
      pc = 0xF0000 + w;
   else
      pc = (b * 0x4000) + (w - 0x4000);
   pc = MAKE_ADDR (1, pc);
   print_addr (pc);
}

void command_change_thread (void)
{
   char eflag = 0;
   target_addr_t addr = (target_addr_t) target_read(thread_current, thread_id_size);
   absolute_address_t th = to_absolute (addr);

   if (th == thread_id)
      return;
   thread_id = th;

   /* TODO thread_debug does not exist by default
      so this will return an error.. which we'll ignore.
   */
   if (machine->dump_thread && eval ("$thread_debug", &eflag))
   {
      if (addr)
      {
         printf ("[Current thread = ");
         print_addr (thread_id);
         machine->dump_thread (thread_id);
         print_thread_data (thread_id);
         printf ("]\n");
      }
      else
      {
         printf ("[ No thread ]\n");
      }
   }
}

/* TODO command_stack and command_stack_* are unused */
void command_stack_push (unsigned int reason)
{
   cmdqueue_t *q = &command_stack[++command_stack_depth];
}

void command_stack_pop (void)
{
   cmdqueue_t *q = &command_stack[command_stack_depth];
   --command_stack_depth;
}

void command_stack_add (const char *cmd)
{
   cmdqueue_t *q = &command_stack[command_stack_depth];
}

char* getarg (void)
{
   return strtok (NULL, " \t\n");
}

/****************** Command Handlers ************************/

void cmd_print (void)
{
   char *arg = getarg ();

   if (arg)
      do_print (arg);
   else
      do_print ("$");
}

void cmd_set (void)
{
   char *arg = getarg ();

   if (!strcmp (arg, "var"))
   {
      /* this form allows the creation of entries
       * in the user symbol table.
       */
      char *p;
      char eflag = 0;
      unsigned long val;

      arg = getarg ();
      if ((p = strchr (arg, '=')) != NULL)
      {
         *p++ = '\0';
         val = eval (p, &eflag); /* Evaluate RHS */
         if (eflag)
            report_errors(eflag);
         else
            sym_set (&internal_symtab, arg, val, 0);
      }
      else
      {
         report_errors(0x80);
      }
   }
   else
   {
      /* this form is a memory write */
      if (arg)
         do_set (arg);
   }
}

void cmd_examine (void)
{
   char eflag = 0;
   char *arg = getarg ();
   if (arg)
      examine_addr = eval_mem (arg, LVALUE, &eflag);

   if (eflag)
      report_errors(eflag);
   else
      do_examine ();
}

void cmd_break (void)
{
   char eflag = 0;
   char *arg = getarg ();

   if (!arg)
      return;

   unsigned long val = eval_mem (arg, LVALUE, &eflag);

   if (eflag)
      report_errors(eflag);
   else
   {
      breakpoint_t *br = brkalloc ();
      br->addr = val;
      br->on_execute = 1;

      arg = getarg ();
      if (!arg);
      else if (!strcmp (arg, "if"))
      {
         br->conditional = 1;
         arg = getarg ();
         strcpy (br->condition, arg);
      }
      else if (!strcmp (arg, "ignore"))
      {
         br->ignore_count = atoi (getarg ());
      }

      brkprint (br);
   }
}

void cmd_watch1 (int on_read, int on_write)
{
   char eflag = 0;
   char *arg = getarg ();

   if (!arg)
      return;

   absolute_address_t addr = eval_mem (arg, LVALUE, &eflag);

   if (eflag)
      report_errors(eflag);
   else
   {
      breakpoint_t *br = brkalloc ();
      br->addr = addr;
      br->on_read = on_read;
      br->on_write = on_write;

      for (;;)
      {
         arg = getarg ();
         if (!arg)
            break;

         if (!strcmp (arg, "print"))
            br->keep_running = 1;
         else if (!strcmp (arg, "mask"))
         {
            arg = getarg ();
            br->write_mask = strtoul (arg, NULL, 0);
         }
         else if (!strcmp (arg, "if"))
         {
            arg = getarg ();
            br->conditional = 1;
            strcpy (br->condition, arg);
         }
      }

      brkprint (br);
   }
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

void cmd_step (void)
{
   char *arg = getarg ();
   if (arg && (auto_break_insn_count = atoi(arg)));
   else
      auto_break_insn_count = 1;

   exit_command_loop = 0;
}

void cmd_next (void)
{
   char buf[128];
   breakpoint_t *br;

   unsigned long addr = to_absolute (get_pc ());
   addr += dasm (buf, addr);

   br = brkalloc ();
   br->addr = addr;
   br->on_execute = 1;
   br->temp = 1;

   /* TODO - for conditional branches, should also set a
      temp breakpoint at the branch target */

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
   unsigned int id;

   if (!arg)
   {
      printf ("Deleting all breakpoints.\n");
      for (id = 0; id < MAX_BREAKS; id++)
      {
         breakpoint_t *br = brkfind_by_id (id);
         brkfree (br);
      }
      return;
   }

   id = atoi (arg);
   breakpoint_t *br = brkfind_by_id (id);
   if (br->used)
   {
      printf ("Deleting breakpoint %d\n", id);
      brkfree (br);
   }
}

void cmd_list (void)
{
   char eflag = 0;
   char *arg = getarg ();
   static absolute_address_t lastpc = 0;
   static absolute_address_t lastaddr = 0;
   absolute_address_t addr;
   int n;

   if (arg)
      addr = eval_mem (arg, LVALUE, &eflag);
   else
   {
      addr = to_absolute (get_pc ());
      if (addr == lastpc)
         addr = lastaddr;
      else
         lastaddr = lastpc = addr;
   }

   if (eflag)
      report_errors(eflag);
   else
   {
      for (n = 0; n < 16; n++)
      {
         addr += print_insn_long(addr);
      }

      lastaddr = addr;
   }
}

void cmd_symbol_file (void)
{
   char *arg = getarg ();
   if (arg)
      load_map_file (arg);
}

void cmd_display (void)
{
   char eflag = 0;
   char *arg;

   while ((arg = getarg ()) != NULL)
   {
      display_t *ds = display_alloc ();
      strcpy (ds->expr, arg);
      ds->type = print_type;
      parse_format_flag (command_flags, &ds->type.format);
      parse_size_flag (command_flags, &ds->type.size);
      if (eflag)
      {
         report_errors(eflag);
         break;
      }
   }
}

int command_exec_file (const char *filename)
{
   FILE *infile;
   extern int command_exec (FILE *);

   infile = file_open (NULL, filename, "r");
   if (!infile)
      return 0;

   command_input = infile;
   return 1;
}

void cmd_source (void)
{
   char *arg = getarg ();
   if (!arg)
      return;

   if (command_exec_file (arg) == 0)
      fprintf (stderr, "can't open %s\n", arg);
}

void cmd_regs (void)
{
   print_regs();
}

void cmd_pc(void)
{
   char eflag = 0;
   unsigned long val;
   char* arg = getarg();

   if (!arg)
      return;

   val = eval_mem(arg, LVALUE, &eflag);

   if (eflag)
      report_errors(eflag);
   else
   {
      set_pc(val);
      cmd_list();
   }
}

void cmd_vars (void)
{
   struct symtab *symtab = &program_symtab; /* default */
   char* arg = getarg();
   if (arg && !strcmp(arg, "auto"))
   {
      symtab = &auto_symtab;
      printf("Print auto\n");
   }
   else if (arg && !strcmp(arg, "internal"))
   {
      symtab = &internal_symtab;
      printf("Print internal\n");
   }
   symtab_print (symtab);
}

void cmd_runfor (void)
{
   char eflag = 0;
   char *units;
   char *arg = getarg ();

   if (!arg)
      return;

   unsigned long val = atoi (arg);

   /* do the check here because, even if there is
      an error with the new args, we will abandon
      any previous runfor that's in progress
   */
   if (stop_after_ms != 0)
      printf ("Previous 'runfor' abandoned\n");

   units = getarg ();
   if (!units || !strcmp (units, "ms"))
      stop_after_ms = val;
   else if (!strcmp (units, "s"))
      stop_after_ms = val * 1000;
   else
      eflag = 1;

   if (eflag)
      fprintf (stderr, "error: bad time units\n");
   else if (val == 0)
      fprintf (stderr, "error: bad time value\n");
   else
      exit_command_loop = 0;
}

void cmd_measure (void)
{
   char eflag = 0;
   absolute_address_t addr;
   target_addr_t retaddr = get_pc ();
   breakpoint_t *br;

   /* Get the address of the function to be measured. */
   char *arg = getarg ();

   if (!arg)
      return;

   addr = eval_mem (arg, LVALUE, &eflag);
   if (eflag)
      report_errors(eflag);
   else
   {
      printf ("Measuring ");
      print_addr (addr);
      printf (" back to ");
      print_addr (to_absolute (retaddr));
      putchar ('\n');

      /* Push the current PC onto the stack for the
         duration of the measurement. */
      set_s (get_s () - 2);
      write16 (get_s (), retaddr);

      /* Set a temp breakpoint at the current PC, so that
         the measurement will halt. */
      br = brkalloc ();
      br->addr = to_absolute (retaddr);
      br->on_execute = 1;
      br->temp = 1;

      /* Interrupts must be disabled for this to work ! */
      set_cc (get_cc () | 0x50);

      /* Change the PC to the function-under-test. */
      set_pc (addr);

      /* Go! */
      exit_command_loop = 0;
   }
}

void cmd_dump_insns (void)
{
   extern int dump_every_insn;

   char *arg = getarg ();
   if (arg)
      dump_every_insn = strtoul (arg, NULL, 0);
   printf ("Instruction dump is %s\n",
           dump_every_insn ? "on" : "off");
}

void cmd_trace_dump (void)
{
   unsigned int off = (trace_offset + 1) % MAX_TRACE;
   do {
      target_addr_t pc = trace_buffer[off];
      absolute_address_t addr = to_absolute (pc);
      print_insn_long(addr);
      off = (off + 1) % MAX_TRACE;
   } while (off != trace_offset);
   fflush (stdout);
}

void cmd_dump (void)
{
   dump_machine();
}

void cmd_restore (void)
{
   printf("not implemented\n");
}

void cmd_info (void)
{
   describe_machine();
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
     "List all breakpoints/watchpoints" },
   { "d", "delete", cmd_delete,
     "Delete a breakpoint/watchpoint" },
   { "s", "step", cmd_step,
     "Step one (or more) instructions" },
   { "n", "next", cmd_next,
     "Break at the next instruction" },
   { "c", "continue", cmd_continue,
     "Continue the program" },
   { "fg", "foreground", cmd_continue, NULL },
   { "q", "quit", cmd_quit,
     "Quit the simulator" },
   { "re", "reset", cpu_reset,
     "Reset the CPU" },
   { "h", "help", cmd_help,
     "Display this help" },
   { "wa", "watch", cmd_watch,
     "Add a watchpoint on write" },
   { "rwa", "rwatch", cmd_rwatch,
     "Add a watchpoint on read" },
   { "awa", "awatch", cmd_awatch,
     "Add a watchpoint on read/write" },
   { "?", "?", cmd_help },
   { "l", "list", cmd_list },
   { "sym", "symbol-file", cmd_symbol_file,
     "Open a symbol table file" },
   { "di", "display", cmd_display,
     "Add a display expression" },
   { "so", "source", cmd_source,
     "Run a command script" },
   { "regs", "regs", cmd_regs,
     "Show all CPU registers" },
   { "vars", "vars", cmd_vars,
     "Show all program variables" },
   { "runfor", "runfor", cmd_runfor,
     "Run for a certain amount of time" },
   { "me", "measure", cmd_measure,
     "Measure time that a function takes" },
   { "dumpi", "dumpi", cmd_dump_insns,
     "Set dump-instruction flag" },
   { "td", "tracedump", cmd_trace_dump,
     "Dump the trace buffer" },
   { "dump", "du", cmd_dump,
     "Dump contents of memory to a file" },
   { "restore", "res", cmd_restore,
     "Restore contents of memory from a file" },
   { "i", "info", cmd_info,
     "Describe machine, devices and address mapping" },
   { "pc", "pc", cmd_pc,
     "Set program counter" },
#if 0
   { "cl", "clear", cmd_clear },
   { "co", "condition", cmd_condition },
   { "tr", "trace", cmd_trace },
   { "di", "disable", cmd_disable },
   { "en", "enable", cmd_enable },
   { "f", "file", cmd_file,
     "Choose the program to be debugged" },
   { "exe", "exec-file", cmd_exec_file,
     "Open an executable" },
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

command_handler_t command_lookup (const char *cmd)
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
      /* TODO - look for a match anywhere between
       * the minimum prefix and the full name */
      cn++;
   }
   return NULL;
}

static int print_insn_long (absolute_address_t addr)
{
   char buf[64];
   int i;
   int size = dasm(buf, addr);

   const char* name;

   print_device_name(addr >> 28);
   putchar(':');
   printf("0x%04lX ", addr & 0xFFFFFF);

   for (i = 0; i < size; i++)
      printf("%02X", abs_read8(addr + i));

   for (i = 0; i < 4 - size; i++)
      printf("  ");

   name = sym_lookup(&program_symtab, addr);
   if (name)
      printf("  %-12.12s", name);
   else
      printf("%-14.14s", "");

   printf("%s", buf);
   putchar ('\n');
   return size;
}

void print_current_insn (void)
{
   print_insn_long(to_absolute(get_pc()));
}

int command_exec (FILE *infile)
{
   char buffer[256];
   static char prev_buffer[256];
   char *cmd;
   command_handler_t handler;

   do {
      errno = 0;
#ifdef HAVE_READLINE
      if (infile == stdin)
      {
         char *buf = readline ("(dbg) ");
         if (buf == NULL)
            return -1;
         if (*buf)
            add_history (buf);
         strcpy (buffer, buf);
         strcat (buffer, "\n");
      }
      else
#endif
      {
         if (infile == stdin)
            printf ("(dbg) ");
         fgets (buffer, 255, infile);
         if (feof (infile))
            return -1;
      }
   } while (errno != 0);

   /* In terminal mode, a blank line means to execute
      the previous command. */
   if (buffer[0] == '\n')
      strcpy (buffer, prev_buffer);

   /* Skip comments */
   if (*buffer == '#')
      return 0;

   cmd = strtok (buffer, " \t\n");
   if (!cmd)
      return 0;

   strcpy (prev_buffer, cmd);

   handler = command_lookup (cmd);
   if (!handler)
   {
      syntax_error ("no such command");
      return 0;
   }

   (*handler) ();
   return 0;
}

void keybuffering_defaults (void)
{
#ifndef _MSC_VER

   /* get two copies of the terminal settings for stdin */
   tcgetattr(STDIN_FILENO,&old_tio);
   tcgetattr(STDIN_FILENO,&new_tio);

   /* disable canonical mode (buffered i/o) and local echo */
   new_tio.c_lflag &=(~ICANON & ~ECHO);

#endif
}

void keybuffering (int flag)
{
#ifndef _MSC_VER
   if (flag) {
      tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
   }
   else {
      tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);
   }
#endif
}


/* Non-blocking check for input character. If
 *   true, retrieve character using kbchar()
 */
int kbhit(void)
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int kbchar(void)
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}

int command_loop (void)
{
   keybuffering (1);
   brkfree_temps ();

  restart:
   if (command_input == stdin)
   {
      display_print ();
      print_current_insn ();
   }

   exit_command_loop = -1;
   while (exit_command_loop < 0)
   {
      if (command_exec (command_input) < 0)
         break;
   }

   if (exit_command_loop == 0)
      keybuffering (0);

   if (feof (command_input) && command_input != stdin)
   {
      fclose (command_input);
      command_input = stdin;
      goto restart;
   }

   return (exit_command_loop);
}

void breakpoint_hit (breakpoint_t *br)
{
   /* TODO don't know how best to handle errors here. */
   char eflag = 0; /* unused */
   if (br->threaded && (thread_id != br->tid))
      return;

   if (br->conditional)
   {
      if (eval (br->condition, &eflag) == 0)
         return;
   }

   if (br->ignore_count)
   {
      --br->ignore_count;
      return;
   }

   monitor_on = !br->keep_running;
}

void command_trace_insn (target_addr_t addr)
{
   trace_buffer[trace_offset++] = addr;
   trace_offset %= MAX_TRACE;
}

void command_insn_hook (void)
{
   target_addr_t pc;
   absolute_address_t abspc;
   breakpoint_t *br;

   pc = get_pc ();
   command_trace_insn (pc);

   if (active_break_count == 0)
      return;

   abspc = to_absolute (pc);
   br = brkfind_by_addr (abspc);
   if (br && br->enabled && br->on_execute)
   {
      breakpoint_hit (br);
      if (monitor_on == 0)
         return;
      if (br->temp)
         brkfree (br);
      else
         printf ("Breakpoint %d reached.\n", br->id);
   }
}

void command_read_hook (absolute_address_t addr)
{
   breakpoint_t *br;

   if (active_break_count == 0)
      return;

   br = brkfind_by_addr (addr);
   if (br && br->enabled && br->on_read)
   {
      printf ("Watchpoint %d triggered. [pc=0x%04X ", br->id, get_pc());
      print_addr (addr);
      printf ("]\n");
      breakpoint_hit (br);
   }
}

void command_write_hook (absolute_address_t addr, U8 val)
{
   breakpoint_t *br;

   if (active_break_count != 0)
   {
      br = brkfind_by_addr (addr);
      if (br && br->enabled && br->on_write)
      {
         if (br->write_mask)
         {
            int mask_ok = ((br->last_write & br->write_mask) !=
                           (val & br->write_mask));
            br->last_write = val;
            if (!mask_ok)
               return;
         }

         breakpoint_hit (br);

         printf ("Watchpoint %d triggered. [pc=0x%04X ", br->id, get_pc());
         print_addr (addr);
         printf (" = 0x%02X]\n", val);
      }
   }

   /* On any write, if threading is enabled then see if the
    * thread ID changed by re-reading it from the target. */
   if (thread_id_size && (addr == thread_current + thread_id_size - 1))
   {
      command_change_thread ();
   }
}

void command_periodic (void)
{
   if (stop_after_ms)
   {
      stop_after_ms -= 100;
      if (stop_after_ms <= 0)
      {
         monitor_on = 1;
         stop_after_ms = 0;
         printf ("Stopping after time elapsed.\n");
      }
   }
}

void pc_virtual (unsigned long *val, int writep) {
   if (writep) set_pc (*val);
   else *val = get_pc ();
}
void x_virtual (unsigned long *val, int writep) {
   if (writep) set_x (*val);
   else *val = get_x ();
}
void y_virtual (unsigned long *val, int writep) {
   if (writep)
      set_y (*val);
   else *val = get_y ();
}
void u_virtual (unsigned long *val, int writep) {
   if (writep)
      set_u (*val);
   else
      *val = get_u ();
}
void s_virtual (unsigned long *val, int writep) {
   if (writep)
      set_s (*val);
   else
      *val = get_s ();
}
void d_virtual (unsigned long *val, int writep) {
   if (writep)
      set_d (*val);
   else
      *val = get_d ();
}
void a_virtual (unsigned long *val, int writep) {
   if (writep)
      set_a (*val);
   else
      *val = get_a ();
}
void b_virtual (unsigned long *val, int writep) {
   if (writep)
      set_b (*val);
   else
      *val = get_b ();
}
void dp_virtual (unsigned long *val, int writep) {
   if (writep)
      set_dp (*val);
   else
      *val = get_dp ();
}
void cc_virtual (unsigned long *val, int writep) {
   if (writep)
      set_cc (*val);
   else
      *val = get_cc ();
}
void irq_load_virtual (unsigned long *val, int writep) {
   if (!writep)
      *val = irq_cycles / IRQ_CYCLE_COUNTS;
}

void cycles_virtual (unsigned long *val, int writep)
{
   if (!writep)
      *val = get_cycles ();
}

void et_virtual (unsigned long *val, int writep)
{
   static unsigned long last_cycles = 0;
   if (!writep)
      *val = get_cycles () - last_cycles;
   last_cycles = get_cycles ();
}

/**
 * Update the $irqload virtual register, which tracks the
 * average number of cycles spent in IRQ.  This function
 * maintains a rolling history of IRQ_CYCLE_COUNTS entries.
 */
void command_exit_irq_hook (unsigned long cycles)
{
   irq_cycles -= irq_cycle_tab[irq_cycle_entry];
   irq_cycles += cycles;
   irq_cycle_tab[irq_cycle_entry] = cycles;
   irq_cycle_entry = (irq_cycle_entry + 1) % IRQ_CYCLE_COUNTS;
}

void command_init (void)
{
   /* Install virtual registers.  These are referenced in expressions
    * using a dollar-sign prefix (e.g. $pc).  The value of the
    * symbol is a pointer to a function (e.g. pc_virtual) which
    * computes the value dynamically. */
   sym_add (&auto_symtab, "pc", (unsigned long)pc_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "x", (unsigned long)x_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "y", (unsigned long)y_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "u", (unsigned long)u_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "s", (unsigned long)s_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "d", (unsigned long)d_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "a", (unsigned long)a_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "b", (unsigned long)b_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "dp", (unsigned long)dp_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "cc", (unsigned long)cc_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "cycles", (unsigned long)cycles_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "et", (unsigned long)et_virtual, SYM_AUTO);
   sym_add (&auto_symtab, "irqload", (unsigned long)irq_load_virtual, SYM_AUTO);

   examine_type.format = 'X'; /* hex with upper-case A-F */
   examine_type.size = 1;

   print_type.format = 'X';
   print_type.size = 1;

   command_input = stdin;
   (void)command_exec_file (".dbinit");
}
