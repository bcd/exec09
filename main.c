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
unsigned long max_cycles = 500000000UL;

/* When nonzero, says that the state of the machine is persistent
across runs of the simulator. */
int machine_persistent = 0;

/* When nonzero, says that the simulator is slowed down to match what a real
processor would run like. */
int machine_realtime = 0;

static int type = S19;

char *exename;

const char *machine_name = "simple";

const char *prog_name = NULL;

FILE *stat_file = NULL;

struct timeval time_started;


/**
 * Return elapsed real time in milliseconds.
 */
long
time_diff (struct timeval *old, struct timeval *new)
{
	long ms = (new->tv_usec - old->tv_usec) / 1000;
	ms += (new->tv_sec - old->tv_sec) * 1000;
	return ms;
}


long
get_elapsed_realtime (void)
{
	struct timeval now;
	gettimeofday (&now, NULL);
	return time_diff (&time_started, &now);
}


/*
 * Check if the CPU should idle.
 */
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
	static int period = 30;
	static int count = 30;
	int delay;
	static int total_ms_elapsed = 0;
	static int cumulative_delay = 0;

	if (--count > 0)
		return;

	if (last.tv_sec == 0 && last.tv_usec == 0)
		gettimeofday (&last, NULL);

	gettimeofday (&now, NULL);
	real_ms = time_diff (&last, &now);
	last = now;

	cycles = get_cycles ();
	sim_ms = (cycles - last_cycles) / cycles_per_ms;
	if (sim_ms < 0)
		sim_ms += cycles_per_ms;
	last_cycles = cycles;

	total_ms_elapsed += sim_ms;
	if (total_ms_elapsed > 100)
	{
		total_ms_elapsed -= 100;
		if (machine->periodic)
			machine->periodic ();
		command_periodic ();
	}

	delay = sim_ms - real_ms;
	cumulative_delay += delay;
	if (cumulative_delay > 0)
	{
		usleep (50 * 1000UL);
		cumulative_delay -= 50;
	}

	count = period;
}


int do_help (const char *arg __attribute__((unused)));

#define NO_NEG    0
#define HAS_NEG   1

#define NO_ARG    0
#define HAS_ARG   1


struct option
{
	char o_short;
	const char *o_long;
	const char *help;
	unsigned int can_negate : 1;
	unsigned int takes_arg : 1;
	int *int_value;
	int default_value;
	const char **string_value;
	int (*handler) (const char *arg);
} option_table[] = {
	{ 'd', "debug", "Enter the monitor immediately",
		HAS_NEG, NO_ARG, &debug_enabled, 1, NULL, NULL },
	{ 'h', "help", NULL,
		NO_NEG, NO_ARG, NULL, 0, 0, do_help },
	{ 'b', "binary", "",
		NO_NEG, NO_ARG, &type, BIN, NULL, NULL },
	{ 'M', "mhz", "", NO_NEG, HAS_ARG },
	{ '-', "68a09", "Emulate the 68A09 variation (1.5Mhz)" },
	{ '-', "68b09", "Emulate the 68B09 variation (2Mhz)" },
	{ 'R', "realtime", "Limit simulation speed to match realtime",
		HAS_NEG, NO_ARG, &machine_realtime, 0, NULL, NULL },
	{ 'I', "irqfreq", "Asserts an IRQ every so many cycles automatically",
		NO_NEG, HAS_ARG, &cycles_per_irq, 0, NULL, NULL },
	{ 'F', "firqfreq", "Asserts an FIRQ every so many cycles automatically",
		NO_NEG, HAS_ARG, &cycles_per_firq, 0, NULL, NULL },
	{ 'C', "cycledump", "",
		HAS_NEG, NO_ARG, &dump_cycles_on_success, 1, NULL, NULL},
	{ 't', "loadmap", "" },
	{ 'T', "trace", "",
		NO_NEG, NO_ARG, &trace_enabled, 1, NULL, NULL },
	{ 'm', "maxcycles", "Sets maximum number of cycles to run",
		NO_NEG, HAS_ARG, &max_cycles, 0, NULL, NULL },
	{ 's', "machine", "Specify the machine (exact hardware) to emulate",
		NO_NEG, HAS_ARG, NULL, 0, &machine_name, NULL },
	{ 'p', "persistent", "Use persistent machine state",
		NO_NEG, NO_ARG, &machine_persistent, 1, NULL, NULL },
	{ '\0', NULL },
};


int
do_help (const char *arg __attribute__((unused)))
{
	struct option *opt = option_table;

	printf ("Motorola 6809 Simulator     Version %s\n", PACKAGE_VERSION);
	printf ("m6809-run [options] [program]\n\n");
	printf ("Options:\n");
	while (opt->o_long != NULL)
	{
		if (opt->help)
		{
			if (opt->o_short == '-')
				printf ("   --%-16.16s    %s\n", opt->o_long, opt->help);
			else
				printf ("   -%c, --%-16.16s%s\n", opt->o_short, opt->o_long, opt->help);
		}
		opt++;
	}
	exit (0);
}


void usage (void)
{
	do_help (NULL);
}


/**
 * Returns positive if an argument was taken.
 * Returns zero if no argument was taken.
 * Returns negative on error.
 */
int
process_option (struct option *opt, const char *arg)
{
	int rc;
	//printf ("Processing option '%s'\n", opt->o_long);
	if (opt->takes_arg)
	{
		if (!arg)
		{
			//printf ("  Takes argument but none given.\n");
			rc = 0;
		}
		else
		{
			if (opt->int_value)
			{
				*(opt->int_value) = strtoul (arg, NULL, 0);
				//printf ("  Integer argument '%d' taken.\n", *(opt->int_value));
			}
			else if (opt->string_value)
			{
				*(opt->string_value) = arg;
				//printf ("  String argument '%s' taken.\n", *(opt->string_value));
			}
			rc = 1;
		}
	}
	else
	{
		if (arg)
			//printf ("  Takes no argument but one given, ignored.\n");

		if (opt->int_value)
		{
			*(opt->int_value) = opt->default_value;
			//printf ("  Integer argument 1 implied.\n");
		}
		rc = 0;
	}

	if (opt->handler)
	{
		rc = opt->handler (arg);
		//printf ("  Handler called, rc=%d\n", rc);
	}

	if (rc < 0)
		sim_exit (0x70);
	return rc;
}


int
process_plain_argument (const char *arg)
{
	//printf ("plain argument '%s'\n", arg);
	if (!prog_name)
		prog_name = arg;
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
						argn++;
						(void)process_option (opt, rest);
						goto next_arg;
					}
					opt++;
				}
				printf ("long option '%s' not recognized.\n", arg+2);
			}
			else
			{
				opt = option_table;
				while (opt->o_long != NULL)
				{
					if (opt->o_short == arg[1])
					{
						argn++;
						if (process_option (opt, argv[argn]))
							argn++;
						goto next_arg;
					}
					opt++;
				}
				printf ("short option '%c' not recognized.\n", arg[1]);
			}
			argn++;
		}
		else
		{
			process_plain_argument (argv[argn++]);
		}
	}
}



int
main (int argc, char *argv[])
{
  int off = 0;
  int i, j, n;
  int argn = 1;
  unsigned int loops = 0;

	gettimeofday (&time_started, NULL);

  exename = argv[0];
  /* TODO - enable different options by default
  based on the executable name. */

	parse_args (argc, argv);

	sym_init ();

	switch (type)
	{
		case HEX:
			if (prog_name && load_hex (prog_name))
				usage ();
			break;

		case S19:
			/* The machine loader cannot deal with S-record files.
			So initialize the machine first, passing it a NULL
			filename, then load the S-record file afterwards. */
			machine_init (machine_name, NULL);
			if (prog_name && load_s19 (prog_name))
				usage ();
			break;

		default:
			machine_init (machine_name, prog_name);
			break;
	}

	/* Try to load a map file */
	if (prog_name)
		load_map_file (prog_name);

	/* Enable debugging if no executable given yet. */
	if (!prog_name)
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
			/* Simulate some CPU time, either 1ms worth or up to the
			next possible IRQ */
			total += cpu_execute (mhz * 1024);

			/* Call each device that needs periodic processing. */
			machine_update ();
		}
		else
		{
			total += cpu_execute (cycles_per_irq);
			/* TODO - this assumes periodic interrupts (WPC) */
			request_irq (0);
			{
			/* TODO - FIRQ frequency not handled yet */
				static int firq_freq = 0;
				if (++firq_freq == 8)
				{
					request_firq (0);
					firq_freq = 0;
				}
			}
		}

		idle_loop ();

		/* Check for a rogue program that won't end */
		if ((max_cycles > 0) && (total > max_cycles))
		{
			sim_error ("maximum cycle count exceeded at %s\n",
				monitor_addr_name (get_pc ()));
		}
	}

	sim_exit (0);
	return 0;
}
