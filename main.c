/*
 * Copyright 2001 by Arto Salmi and Joze Fabcic
 * Copyright 2006-2008 by Brian Dominy <brian@oddchange.com>
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


#include <sys/time.h>
#include "6809.h"

enum
{ HEX, S19, BIN };


/* The total number of cycles that have executed */
unsigned long total = 0;

/* The frequency of the emulated CPU, in megahertz */
unsigned int mhz = 1;

/* When nonzero, indicates that the IRQ should be triggered periodically,
every so many cycles.  By default no periodic IRQ is generated. */
unsigned int cycles_per_irq = 0;

/* When nonzero, indicates that the FIRQ should be triggered periodically,
every so many cycles.  By default no periodic FIRQ is generated. */
unsigned int cycles_per_firq = 0;

/* Nonzero if debugging support is turned on */
int debug_enabled = 0;

/* Nonzero if tracing is enabled */
int trace_enabled = 0;

/* When nonzero, causes the program to print the total number of cycles
on a successful exit. */
int dump_cycles_on_success = 0;

/* When nonzero, indicates the total number of cycles before an automated
exit.  This is to help speed through test cases that never finish. */
int max_cycles = 100000000;

char *exename;

const char *machine_name = "simple";


static void usage (void)
{
  printf ("Usage: %s <options> filename\n", exename);
  printf ("Options are:\n");
  printf ("-hex	- load intel hex file\n");
  printf ("-s19	- load motorola s record file\n");
  printf ("-bin	- load binary file\n");
  printf ("-s addr - specify binary load address hexadecimal (default 0)\n");
  printf ("default format is motorola s record\n");
  exit (1);
}


void
idle_loop (void)
{
	struct timeval now;
	static struct timeval last = { 0, 0 };
	int real_ms;
	static unsigned long last_cycles = 0;
	unsigned long cycles;
	int sim_ms;
	const int cycles_per_ms = 2000;
	static int period = 100;
	static int count = 100;
	int delay;
	static int total_ms_elapsed = 0;

	if (--count > 0)
		return;

	if (last.tv_usec == 0)
		gettimeofday (&last, NULL);

	gettimeofday (&now, NULL);
	real_ms = (now.tv_usec - last.tv_usec) / 1000;
	if (real_ms < 0)
		real_ms += 1000;
	last = now;

	cycles = get_cycles ();
	sim_ms = (cycles - last_cycles) / cycles_per_ms;
	if (sim_ms < 0)
		sim_ms += cycles_per_ms;
	last_cycles = cycles;


	/* Minimum sleep time is usually around 10ms. */
	delay = sim_ms - real_ms;
	total_ms_elapsed += delay;
	if (total_ms_elapsed > 100)
	{
		total_ms_elapsed -= 100;
		wpc_periodic ();
	}

	if (delay > 0)
	{
		if (delay > 60)
			period -= 5;
		else if (delay < 20)
			period += 5;
		usleep (delay * 1000UL);
	}

	count = period;
}



struct option
{
	char o_short;
	const char *o_long;
	const char *help;
	unsigned int can_negate : 1;
	unsigned int takes_arg : 1;
	int *int_value;
	char *string_value;
} option_table[] = {
	{ 'd', "debug" },
	{ 'h', "help" },
	{ 'b', "binary" },
	{ 'M', "mhz" },
	{ '-', "68a09" },
	{ '-', "68b09" },
	{ 'R', "realtime" },
	{ 'I', "irqfreq" },
	{ 'F', "firqfreq" },
	{ 'C', "cycledump" },
	{ 't', "loadmap" },
	{ 'T', "trace" },
	{ 'm', "maxcycles" },
	{ 's', "machine" },
	{ '\0', NULL },
};


int
process_option (struct option *opt, const char *arg)
{
	return 0;
}


int
parse_args (int argc, char *argv[])
{
	int argn = 1;
	struct option *opt;

next_arg:
	while (argn < argc)
	{
		char *arg = argv[argn];
		if (arg[0] == '-')
		{
			if (arg[1] == '-')
			{
				char *rest = strchr (arg+2, '=');
				if (rest)
					*rest++ = '\0';

				opt = option_table;
				while (opt->o_long != NULL)
				{
					if (!strcmp (opt->o_long, arg+2))
					{
						if (process_option (opt, rest))
							argn++;
						goto next_arg;
					}
					opt++;
				}
			}
			else
			{
				opt = option_table;
				while (opt->o_long != NULL)
				{
					if (opt->o_short == arg[1])
					{
						if (process_option (opt, argv[argn]))
							argn++;
						goto next_arg;
					}
					opt++;
				}
			}
		}
		else
		{
			return argn;
		}
	}
}



int
main (int argc, char *argv[])
{
  char *name = NULL;
  int type = S19;
  int off = 0;
  int i, j, n;
  int argn = 1;
  int load_tmp_map = 0;
  unsigned int loops = 0;

  exename = argv[0];
  /* TODO - enable different options by default
  based on the executable name. */

  while (argn < argc)
    {
      if (argv[argn][0] == '-')
	{
	  int argpos = 1;
	next_arg:
	  switch (argv[argn][argpos++])
	    {
	    case 'd':
	      debug_enabled = 1;
	      goto next_arg;
	    case 'h':
	      type = HEX;
	      goto next_arg;
	    case 'b':
	      type = BIN;
	      goto next_arg;
	    case 'M':
	      mhz = strtoul (argv[++argn], NULL, 16);
	      break;
	    case 'o':
	      off = strtoul (argv[++argn], NULL, 16);
	      type = BIN;
	      break;
	    case 'I':
	      cycles_per_irq = strtoul (argv[++argn], NULL, 0);
	      break;
	    case 'F':
	      cycles_per_firq = strtoul (argv[++argn], NULL, 0);
	      break;
	    case 'C':
	      dump_cycles_on_success = 1;
	      goto next_arg;
       case 't':
		   load_tmp_map = 1;
			break;
       case 'T':
		   trace_enabled = 1;
			goto next_arg;
       case 'm':
		   max_cycles = strtoul (argv[++argn], NULL, 16);
			break;
		case 's':
			machine_name = argv[++argn];
			break;
	    case '\0':
	      break;
	    default:
	      usage ();
	    }
	}
      else if (!name)
	{
	  name = argv[argn];
	}
      else
	{
	  usage ();
	}
      argn++;
    }

	sym_init ();

	switch (type)
	{
		case HEX:
			if (name && load_hex (name))
				usage ();
			break;

		case S19:
			machine_init (machine_name, NULL);
			if (name && load_s19 (name))
				usage ();
			break;

		default:
			machine_init (machine_name, name);
			break;
	}

	/* Try to load a map file */
	if (load_tmp_map)
		load_map_file ("tmp");
	else if (name)
		load_map_file (name);

	/* Enable debugging if no executable given yet. */
	if (!name)
		debug_enabled = 1;
	else
		/* OK, ready to run.  Reset the CPU first. */
		cpu_reset ();

	monitor_init ();
	command_init ();
   keybuffering (0);

	/* Now, iterate through the instructions.
	 * If no IRQs or FIRQs are enabled, we can just call cpu_execute()
	 * and let it run for a long time; otherwise, we need to come back
	 * here periodically and do the interrupt handling. */
	for (cpu_quit = 1; cpu_quit != 0;)
	{
   	if ((cycles_per_irq == 0) && (cycles_per_firq == 0))
		{
			total += cpu_execute (max_cycles ? max_cycles-1 : 500000);
		}
		else
		{
			/* TODO - FIRQ not handled yet */
			total += cpu_execute (cycles_per_irq);
			request_irq (0);
		}

		idle_loop ();

		/* Check for a rogue program that won't end */
		if ((max_cycles > 0) && (total > max_cycles))
		{
			sim_error ("maximum cycle count exceeded at %s\n",
				monitor_addr_name (get_pc ()));
		}
	}
	printf ("m6809-run stopped after %d cycles\n", total);
	return 0;
}
