/*
 * Copyright 2006, 2007 by Brian Dominy <brian@oddchange.com>
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

#include "6809.h"
#include <sys/time.h>

#define WPC_RAM_BASE                0x0000
#define WPC_RAM_SIZE                0x2000
#define WPC_ROM_SIZE                0x100000
#define WPC_PAGED_REGION            0x4000
#define WPC_PAGED_SIZE              0x4000
#define WPC_FIXED_REGION            0x8000
#define WPC_FIXED_SIZE              0x8000

/* The register set of the WPC ASIC */
#define WPC_ASIC_BASE               0x3800

#define WPC_DMD_LOW_BASE 				0x3800
#define WPC_DMD_HIGH_BASE 				0x3A00

#define WPC_DEBUG_DATA_PORT			0x3D60
#define WPC_DEBUG_CONTROL_PORT		0x3D61
#define WPC_DEBUG_WRITE_READY   0x1
#define WPC_DEBUG_READ_READY    0x2
#define WPC_PINMAME_CYCLE_COUNT		0x3D62
#define WPC_PINMAME_FUNC_ENTRY_HI	0x3D63
#define WPC_PINMAME_FUNC_ENTRY_LO	0x3D64
#define WPC_PINMAME_FUNC_EXIT_HI		0x3D65
#define WPC_PINMAME_FUNC_EXIT_LO		0x3D66
#define WPC_SERIAL_CONTROL_PORT 		0x3E66
#define WPC_SERIAL_DATA_PORT 			0x3E67

#define WPC_DMD_3200_PAGE				0x3FB8
#define WPC_DMD_3000_PAGE				0x3FB9
#define WPC_DMD_3600_PAGE				0x3FBA
#define WPC_DMD_3400_PAGE				0x3FBB
#define WPC_DMD_HIGH_PAGE 				0x3FBC
#define WPC_DMD_FIRQ_ROW_VALUE 		0x3FBD
#define WPC_DMD_LOW_PAGE 				0x3FBE
#define WPC_DMD_ACTIVE_PAGE 			0x3FBF
#define WPC_SERIAL_STATUS_PORT 		0x3FC0
#define WPC_PARALLEL_DATA_PORT 		0x3FC1
#define WPC_PARALLEL_STROBE_PORT 	0x3FC2
#define WPC_SERIAL_DATA_OUTPUT 		0x3FC3
#define WPC_SERIAL_CONTROL_OUTPUT	0x3FC4
#define WPC_SERIAL_BAUD_SELECT 		0x3FC5
#define WPC_TICKET_DISPENSE 			0x3FC6
#define WPC_DCS_SOUND_DATA_OUT 		0x3FD0
#define WPC_DCS_SOUND_DATA_IN 		0x3FD1
#define WPC_DCS_SOUND_RESET 			0x3FD2
#define WPC_DCS_SOUND_DATA_READY 	0x3FD3
#define WPC_FLIPTRONIC_PORT_A 		0x3FD4
#define WPC_FLIPTRONIC_PORT_B 		0x3FD5
#define WPCS_DATA 						0x3FDC
#define WPCS_CONTROL_STATUS 			0x3FDD
#define WPC_SOL_FLASH2_OUTPUT 		0x3FE0
#define WPC_SOL_HIGHPOWER_OUTPUT 	0x3FE1
#define WPC_SOL_FLASH1_OUTPUT 		0x3FE2
#define WPC_SOL_LOWPOWER_OUTPUT 		0x3FE3
#define WPC_LAMP_ROW_OUTPUT 			0x3FE4
#define WPC_LAMP_COL_STROBE 			0x3FE5
#define WPC_GI_TRIAC 					0x3FE6
#define WPC_SW_JUMPER_INPUT 			0x3FE7
#define WPC_SW_CABINET_INPUT 			0x3FE8
#define WPC_SW_ROW_INPUT 				0x3FE9
#define WPC_SW_COL_STROBE 				0x3FEA
#if (MACHINE_PIC == 1)
#define WPCS_PIC_READ 					0x3FE9
#define WPCS_PIC_WRITE 					0x3FEA
#endif
#if (MACHINE_DMD == 0)
#define WPC_ALPHA_POS 					0x3FEB
#define WPC_ALPHA_ROW1 					0x3FEC
#else
#define WPC_EXTBOARD1 					0x3FEB
#define WPC_EXTBOARD2 					0x3FEC
#define WPC_EXTBOARD3 					0x3FED
#endif
#if (MACHINE_WPC95 == 1)
#define WPC95_FLIPPER_COIL_OUTPUT 	0x3FEE
#define WPC95_FLIPPER_SWITCH_INPUT 	0x3FEF
#else
#endif
#if (MACHINE_DMD == 0)
#define WPC_ALPHA_ROW2 					0x3FEE
#else
#endif
#define WPC_LEDS 							0x3FF2
#define WPC_RAM_BANK 					0x3FF3
#define WPC_SHIFTADDR 					0x3FF4
#define WPC_SHIFTBIT 					0x3FF6
#define WPC_SHIFTBIT2 					0x3FF7
#define WPC_PERIPHERAL_TIMER_FIRQ_CLEAR 0x3FF8
#define WPC_ROM_LOCK 					0x3FF9
#define WPC_CLK_HOURS_DAYS 			0x3FFA
#define WPC_CLK_MINS 					0x3FFB
#define WPC_ROM_BANK 					0x3FFC
#define WPC_RAM_LOCK 					0x3FFD
#define WPC_RAM_LOCKSIZE 				0x3FFE
#define WPC_ZEROCROSS_IRQ_CLEAR 		0x3FFF


struct wpc_asic
{
	struct hw_device *rom_dev;
	struct hw_device *ram_dev;

	U8 led;
	U8 rombank;
	U8 ram_unlocked;
	U8 ram_lock_size;
	U16 shiftaddr;
	U16 shiftbit;
   U8 lamp_strobe;
   U8 lamp_mx[8];
   U8 sols[6];
   U8 switch_strobe;
   U8 switch_mx[8];
   U8 directsw;

	int curr_sw;
	int curr_sw_time;
};

struct wpc_asic *global_wpc;


void wpc_asic_reset (struct hw_device *dev)
{
   struct wpc_asic *wpc = dev->priv;
	global_wpc = wpc;
	wpc->curr_sw_time = 0;
}

static int wpc_console_inited = 0;

static U8 wpc_get_console_state (void)
{
	fd_set fds;
	struct timeval timeout;
	U8 rc = WPC_DEBUG_WRITE_READY;

	if (!wpc_console_inited)
		rc |= WPC_DEBUG_READ_READY;

#if 1
	return rc;
#endif

	FD_ZERO (&fds);
	FD_SET (0, &fds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fds, NULL, NULL, &timeout))
		rc |= WPC_DEBUG_READ_READY;

	return rc;
}


static U8 wpc_console_read (void)
{
	int rc;
	U8 c = 0;

	if (!wpc_console_inited)
	{
		wpc_console_inited = 1;
		return 0;
	}

	rc = read (0, &c, 1);
	return c;
}


static void wpc_console_write (U8 val)
{
	putchar (val);
	fflush (stdout);
}


static int scanbit (U8 val)
{
   if (val & 0x80) return 7;
   else if (val & 0x40) return 6;
   else if (val & 0x20) return 5;
   else if (val & 0x10) return 4;
   else if (val & 0x08) return 3;
   else if (val & 0x04) return 2;
   else if (val & 0x02) return 1;
   else if (val & 0x01) return 0;
   else return -1;
}


unsigned int wpc_read_switch (struct wpc_asic *wpc, int num)
{
	unsigned int val;
	val = wpc->switch_mx[num / 8] & (1 << (num % 8));
	// printf ("SW %d = %d\n", num, val);
	return val ? 1 : 0;
}

void wpc_write_switch (struct wpc_asic *wpc, int num, int flag)
{
	unsigned int col, val;

	col = num / 8;
	val = 1 << (num % 8);
	wpc->switch_mx[col] &= ~val;
	if (flag)
		wpc->switch_mx[col] |= val;
}

void wpc_press_switch (struct wpc_asic *wpc, int num, int delay)
{
	wpc_write_switch (wpc, num, 1);
	wpc->curr_sw = num;
	wpc->curr_sw_time = delay;
}

unsigned int wpc_read_switch_column (struct wpc_asic *wpc, int col)
{
	unsigned int val = 0;
	int row;
	for (row = 0; row < 8; row++)
		if (wpc_read_switch (wpc, col * 8 + row))
			val |= (1 << row);
	return val;
}

void wpc_write_lamp (int num, int flag)
{
}


void wpc_write_sol (int num, int flag)
{
}


void wpc_keypoll (struct wpc_asic *wpc)
{
	fd_set fds;
	struct timeval timeout;
	int rc;
	unsigned char c;

	FD_ZERO (&fds);
	FD_SET (0, &fds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	if (select (1, &fds, NULL, NULL, &timeout))
	{
		rc = read (0, &c, 1);

		//printf ("%c pressed\n", c);
		switch (c)
		{
			case '7':
				wpc_press_switch (wpc, 4, 200);
				break;
			case '8':
				wpc_press_switch (wpc, 5, 200);
				break;
			case '9':
				wpc_press_switch (wpc, 6, 200);
				break;
			case '0':
				wpc_press_switch (wpc, 7, 200);
				break;
			default:
				break;
		}
	}
}



U8 wpc_asic_read (struct hw_device *dev, unsigned long addr)
{
	struct wpc_asic *wpc = dev->priv;
	U8 val;

	switch (addr + WPC_ASIC_BASE)
	{
		case WPC_LEDS:
			val = wpc->led;
			break;

		case WPC_ROM_BANK:
			val = wpc->rombank;
			break;

		case WPC_DEBUG_CONTROL_PORT:
			val = wpc_get_console_state ();
			break;

		case WPC_DEBUG_DATA_PORT:
			val = wpc_console_read ();
			break;

		case WPC_SHIFTADDR:
			val = wpc->shiftaddr >> 8;
			break;

		case WPC_SHIFTADDR+1:
			val = (wpc->shiftaddr & 0xFF) + (wpc->shiftbit % 8);
			break;

		case WPC_SHIFTBIT:
			val = 1 << (wpc->shiftbit % 8);
			break;

		case WPC_SW_ROW_INPUT:
			val = wpc_read_switch_column (wpc, 1 + scanbit (wpc->switch_strobe));
			break;

		case WPC_SW_JUMPER_INPUT:
			val = 0x55;
			break;

		case WPC_SW_CABINET_INPUT:
			val = wpc_read_switch_column (wpc, 0);
			break;

		default:
			val = 0;
			break;
	}
	//printf (">>> ASIC read %04X -> %02X\n", addr + WPC_ASIC_BASE, val);
	return val;
}


/**
 * Enforce the current read-only area of RAM.
 */
void wpc_update_ram (struct wpc_asic *wpc)
{
	unsigned int size_writable = WPC_RAM_SIZE;

	if (!wpc->ram_unlocked)
	{
		switch (wpc->ram_lock_size)
		{
			default:
				break;
			case 0xF:
				size_writable -= 256;
				break;
			case 0x7:
				size_writable -= 512;
			case 0x3:
				size_writable -= 1024;
				break;
			case 0x1:
				size_writable -= 2048;
				break;
			case 0:
				size_writable -= 4096;
				break;
		}
	}

	bus_map (WPC_RAM_BASE, wpc->ram_dev->devid, 0, size_writable, MAP_READWRITE);
	if (size_writable < WPC_RAM_SIZE)
		bus_map (WPC_RAM_BASE + size_writable, wpc->ram_dev->devid, size_writable,
			WPC_RAM_SIZE - size_writable, MAP_READONLY);
}


void wpc_set_rom_page (unsigned char val)
{
	bus_map (WPC_PAGED_REGION, 2, val * WPC_PAGED_SIZE, WPC_PAGED_SIZE, MAP_READONLY);
}


void wpc_asic_write (struct hw_device *dev, unsigned long addr, U8 val)
{
	struct wpc_asic *wpc = dev->priv;
	switch (addr + WPC_ASIC_BASE)
	{
		case WPC_LEDS:
			wpc->led = val;
			break;

		case WPC_ZEROCROSS_IRQ_CLEAR:
			/* ignore for now */
			break;

		case WPC_ROM_BANK:
			wpc->rombank = val;
			wpc_set_rom_page (val);
			break;

		case WPC_DEBUG_DATA_PORT:
			wpc_console_write (val);
			break;

		case WPC_RAM_LOCK:
			wpc->ram_unlocked = val;
			wpc_update_ram (wpc);
			break;

		case WPC_RAM_LOCKSIZE:
			wpc->ram_lock_size = val;
			wpc_update_ram (wpc);
			break;

		case WPC_SHIFTADDR:
			wpc->shiftaddr &= 0x00FF;
			wpc->shiftaddr |= val << 8;
			break;

		case WPC_SHIFTADDR+1:
			wpc->shiftaddr &= 0xFF00;
			wpc->shiftaddr |= val;
			break;

		case WPC_SHIFTBIT:
			wpc->shiftbit = val;
			break;

      case WPC_LAMP_ROW_OUTPUT:
         wpc->lamp_mx[scanbit (wpc->lamp_strobe)] = val;
         break;

      case WPC_LAMP_COL_STROBE:
         wpc->lamp_strobe = val;
         break;

		case WPC_SW_COL_STROBE:
			wpc->switch_strobe = val;

		default:
			break;
	}
	//printf (">>> ASIC write %04X %02X\n", addr + WPC_ASIC_BASE, val);
}


void wpc_periodic (void)
{
	struct wpc_asic *wpc = global_wpc;
	//printf ("WPC 100ms periodic\n");

	wpc_keypoll (wpc);

	if (wpc->curr_sw_time > 0)
	{
		wpc->curr_sw_time -= 100;
		if (wpc->curr_sw_time <= 0)
		{
			wpc->curr_sw_time = 0;
			wpc_write_switch (wpc, wpc->curr_sw, 0);
		}
	}
}


struct hw_class wpc_asic_class =
{
	.reset = wpc_asic_reset,
	.read = wpc_asic_read,
	.write = wpc_asic_write,
};

struct hw_device *wpc_asic_create (void)
{
	struct wpc_asic *wpc = calloc (sizeof (struct wpc_asic), 1);
	return device_attach (&wpc_asic_class, 0x800, wpc);
}


void wpc_fault (unsigned int addr, unsigned char type)
{
}

void io_sym_add (const char *name, unsigned long cpuaddr)
{
   sym_add (&program_symtab, name, to_absolute (cpuaddr), 0);
}

#define IO_SYM_ADD(name) io_sym_add (#name, name)

void wpc_init (const char *boot_rom_file)
{
	struct hw_device *dev;
	struct wpc_asic *wpc;

	device_define ( dev = wpc_asic_create (), 0,
		WPC_ASIC_BASE, WPC_PAGED_REGION - WPC_ASIC_BASE, MAP_READWRITE);
	wpc = dev->priv;

	device_define ( dev = ram_create (WPC_RAM_SIZE), 0,
		WPC_RAM_BASE, WPC_RAM_SIZE, MAP_READWRITE );
	wpc->ram_dev = dev;

	dev = rom_create (boot_rom_file, WPC_ROM_SIZE);
	device_define ( dev, 0,
		WPC_PAGED_REGION, WPC_PAGED_SIZE, MAP_READONLY);
	device_define ( dev, WPC_ROM_SIZE - WPC_FIXED_SIZE,
		WPC_FIXED_REGION, WPC_FIXED_SIZE, MAP_READONLY);
	wpc->rom_dev = dev;
	wpc_update_ram (wpc);

	IO_SYM_ADD(WPC_DMD_HIGH_PAGE);
	IO_SYM_ADD(WPC_DMD_FIRQ_ROW_VALUE);
	IO_SYM_ADD(WPC_DMD_LOW_PAGE);
	IO_SYM_ADD(WPC_DMD_ACTIVE_PAGE);
	IO_SYM_ADD(WPC_SERIAL_STATUS_PORT);
	IO_SYM_ADD(WPC_PARALLEL_DATA_PORT);
	IO_SYM_ADD(WPC_PARALLEL_STROBE_PORT);
	IO_SYM_ADD(WPC_SERIAL_DATA_OUTPUT);
	IO_SYM_ADD(WPC_SERIAL_CONTROL_OUTPUT);
	IO_SYM_ADD(WPC_SERIAL_BAUD_SELECT);
	IO_SYM_ADD(WPC_TICKET_DISPENSE);
	IO_SYM_ADD(WPC_DCS_SOUND_DATA_OUT);
	IO_SYM_ADD(WPC_DCS_SOUND_DATA_IN);
	IO_SYM_ADD(WPC_DCS_SOUND_RESET);
	IO_SYM_ADD(WPC_DCS_SOUND_DATA_READY);
	IO_SYM_ADD(WPC_FLIPTRONIC_PORT_A);
	IO_SYM_ADD(WPC_FLIPTRONIC_PORT_B);
	IO_SYM_ADD(WPCS_DATA);
	IO_SYM_ADD(WPCS_CONTROL_STATUS);
	IO_SYM_ADD(WPC_SOL_FLASH2_OUTPUT);
	IO_SYM_ADD(WPC_SOL_HIGHPOWER_OUTPUT);
	IO_SYM_ADD(WPC_SOL_FLASH1_OUTPUT);
	IO_SYM_ADD(WPC_SOL_LOWPOWER_OUTPUT);
	IO_SYM_ADD(WPC_LAMP_ROW_OUTPUT);
	IO_SYM_ADD(WPC_LAMP_COL_STROBE);
	IO_SYM_ADD(WPC_GI_TRIAC);
	IO_SYM_ADD(WPC_SW_JUMPER_INPUT);
	IO_SYM_ADD(WPC_SW_CABINET_INPUT);
	IO_SYM_ADD(WPC_SW_ROW_INPUT);
	IO_SYM_ADD(WPC_SW_COL_STROBE);
	IO_SYM_ADD(WPC_LEDS);
	IO_SYM_ADD(WPC_RAM_BANK);
	IO_SYM_ADD(WPC_SHIFTADDR);
	IO_SYM_ADD(WPC_SHIFTBIT);
	IO_SYM_ADD(WPC_SHIFTBIT2);
	IO_SYM_ADD(WPC_PERIPHERAL_TIMER_FIRQ_CLEAR);
	IO_SYM_ADD(WPC_ROM_LOCK);
	IO_SYM_ADD(WPC_CLK_HOURS_DAYS);
	IO_SYM_ADD(WPC_CLK_MINS);
	IO_SYM_ADD(WPC_ROM_BANK);
	IO_SYM_ADD(WPC_RAM_LOCK);
	IO_SYM_ADD(WPC_RAM_LOCKSIZE);
}


struct machine wpc_machine =
{
	.name = "wpc",
	.fault = wpc_fault,
	.init = wpc_init,
};



