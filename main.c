/*
    main.c   	-	main program for 6809 simulator
    Copyright (C) 2001  Arto Salmi
                        Joze Fabcic

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "6809.h"

enum { HEX, S19, BIN };

static char *Options[]=
{
 "d","hex","s19","bin","s","mhz","irq","firq",NULL
};

int total = 0;

int mhz = 1;

/* When nonzero, indicates that the IRQ should be triggered periodically,
every so many cycles.  By default no periodic IRQ is generated. */
int cycles_per_irq = 0;

/* When nonzero, indicates that the FIRQ should be triggered periodically,
every so many cycles.  By default no periodic FIRQ is generated. */
int cycles_per_firq = 0;

/* Nonzero if GDB debugging support is turned on */
int debug_enabled = 0;

int need_flush = 0;

int max_cycles = 10000000;

char *exename;

static void usage (void)
{
  printf("Usage: %s <options> filename\n",exename);
  printf("Options are:\n");
  printf("-hex	- load intel hex file\n");
  printf("-s19	- load motorola s record file\n");
  printf("-bin	- load binary file\n");
  printf("-s addr - specify binary load address hexadecimal (default 0)\n");
  printf("default format is motorola s record\n");

  if(memory != NULL) free(memory);
  exit (1);
}


int main (int argc, char *argv[])
{
  char *name;
  int type = S19;
  int off  = 0;
  int i, j, n;

  exename = argv[0];

  if (argc == 1) usage();
 
  for (i=1,n=0;i<argc;++i)
  {
    if (argv[i][0]!='-')
    {
      switch (++n)
      {
        case 1:  name=argv[i]; break;
        default: usage();
      }
    }
    else
    {
      for (j=0;Options[j];j++) if (!strcmp(argv[i]+1,Options[j])) break;
      switch (j)
      {
        case 0:  debug_enabled = 1; break;
        case 1:  type = HEX;  break;
        case 2:  type = S19;  break;
        case 3:  type = BIN;  break;
        case 4:  i++; if (i>argc) usage();
                 off  = strtoul(argv[i],NULL,16);
                 type = BIN;
                 break;
        case 5:  i++; if (i>argc) usage();
                 mhz = strtoul(argv[i],NULL,16);
					  break;
        case 6:  i++; if (i>argc) usage();
                 cycles_per_irq = strtoul(argv[i],NULL,16);
					  break;
        case 7:  i++; if (i>argc) usage();
                 cycles_per_firq = strtoul(argv[i],NULL,16);
					  break;
        default: usage();
      }
    }
  }

  memory = (UINT8 *)malloc(0x10000);

  if (memory == NULL) usage();
  memset (memory, 0, 0x10000);

  cpu_quit = 1;

  switch (type)
  {
    case HEX: if(load_hex(name)) usage(); break;
    case S19: if(load_s19(name)) usage(); break;
    case BIN: if(load_bin(name,off&0xffff)) usage(); break;
  }  

  monitor_init();
  cpu_reset();
  if (debug_enabled)
		gdb_init ();

  do
  {
    if (cycles_per_irq != 0)
	 {
    	total += cpu_execute (cycles_per_irq);
    	irq ();
    }
	 else
	 {
    	total += cpu_execute (10000);
	 }

	if (need_flush)
	{
	 fflush (stdout);
	 need_flush = 0;
	}

	 if (debug_enabled)
	 	gdb_periodic_task ();

	if ((max_cycles > 0) && (total > max_cycles))
	{
		printf ("m6809-run: maximum cycle count exceeded\n");
		exit (100);
	}

  } while (cpu_quit != 0);

  printf("m6809-run stopped after %d cycles\n", total);

  free(memory);

  return 0;
}

