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
 "d","hex","s19","bin","s",NULL
};

int total = 0;

int debug_enabled = 0;

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
					  printf ("off = %X\n", off);
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
  // printf ("AS,JF, 1.2, file %s\n",name);
  do
  {
    total += cpu_execute (60);
  } while (cpu_quit != 0);

  printf("6809 stopped after %d cycles\n",total);

  free(memory);

  return 0;
}
