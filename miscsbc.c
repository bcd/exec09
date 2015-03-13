#include <fcntl.h>
#include "machine.h"
// for symtab stuff?
#include "6809.h"

// for smii console
int smii_i_avail = 1;
int smii_o_busy = 0;


/********************************************************************
 * The Scroungemaster II machine, a platform
 * for 6809 CamelForth. See
 * Brad Rodriguez http://www.camelforth.com/page.php?6
 * and
 * http://www.bradrodriguez.com/papers/impov3.htm
 ********************************************************************/

// by inspection command read (should be address 0x7c02) comes in with addr=0x8d
// TODO no way to check for "char available" and so smii_i_busy is always true and
// console input is blocking.
U8 smii_console_read (struct hw_device *dev, unsigned long addr)
{
    unsigned char ch;
    switch (addr)
    {
    case 0x02: // SCCACMD
        // on output make it seem busy for several polls
        smii_o_busy = smii_o_busy == 0 ? 0 : (smii_o_busy + 1)%4;
        //        printf("02 smii_o_busy = %d return 0x%02x\n",smii_o_busy,(smii_i_avail & 1) | (smii_o_busy == 0 ? 4 : 0));
        return (smii_i_avail & 1) | (smii_o_busy == 0 ? 4 : 0);
    case 0x03: // SCCADTA
        ch = getchar();
        // key conversions to make keyboard look DOS-like
        if (ch == 127) ch = 8; //backspace
        if (ch == 10) ch = 13;    //cr
        return ch;
    default:
        printf("In smii_console_read with addr=0x%08x\n", addr);
        return 0x42;
    }
}


void smii_console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    switch (addr)
	{
        case 0x00:
            // UART setup. Not emulated; just ignore it.
            break;
        case 0x03: // SCCADTA
            if(smii_o_busy != 0) printf("Oops! Write to busy UART\n");
            smii_o_busy = 1;
            putchar(val);
            break;
        default:
            printf("In smii_console_write with addr=0x%08x val=0x%02x\n",addr, val);
        }
}


void quiet_reset (struct hw_device *dev)
{
}

U8 quiet_read (struct hw_device *dev, unsigned long addr)
{
    char *buf = dev->priv;
    char val = buf[addr];
    //    printf("In quiet read with address 0x%x return data 0x%x\n",addr,val);
    return val;
}

// BUG! For image file load, the quiet_write also needs to work, BUT it must
// act like ROM for CPU access. Not sure if that is achieved here.
void quiet_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    char *buf = dev->priv;
    //    printf("In quiet write with address 0x%x data 0x%x\n",addr,val);
    buf[addr]= val;
}

struct hw_class quiet_class =
{
	.name = "quiet-device",
	.readonly = 0,
	.reset = quiet_reset,
	.read = quiet_read,
	.write = quiet_write,
};

struct hw_device *quiet_create (void)
{
	return device_attach (&quiet_class, 0, NULL);
}


void smii_init (const char *boot_rom_file)
{
    struct hw_device *smii_console, *rom, *quiet;

    /* RAM from 0 to 7BFF */
    device_define ( ram_create (0x7C00), 0,
                    0x0000, 0x7C00, MAP_READWRITE );

    /* ROM from E000 to FFFF */
    rom = rom_create (boot_rom_file, 0x2000);
    device_define (rom , 0,
                   0xE000, 0x2000, MAP_READABLE);

    /* The address space 8000-DFFF provides aliases of the ROM
       There is write-only mapping logic for 8 RAM pages and this
       is usually accessed by writes to addresses 8000,9000..F000

       The CamelForth image does writes to those addresses at
       in order to initialise the mapping hardware, but makes
       no further use of it. Since the model is strict the ROM
       is read-only and the write causes a trap.

       To avoid the trap, assign a device that ignores writes
       without error, and maps reads to the ROM.
    */
    quiet = quiet_create();
    /* Allow quiet device to access ROM storage */
    quiet->priv = rom->priv;

    device_define(quiet, 0, 0x8000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0, 0x9000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0, 0xA000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0, 0xB000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0, 0xC000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0, 0xD000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0x0000, 0xE000, BUS_MAP_SIZE, MAP_READWRITE);
    device_define(quiet, 0x1000, 0xF000, BUS_MAP_SIZE, MAP_READWRITE);



    /* Make debug output more informative */
    // ?? haven't seen this work yet..
    sym_add(&internal_symtab, "SCCACMD", to_absolute(0x7c02), 0);
    sym_add(&internal_symtab, "SCCADTA", to_absolute(0x7c03), 0);

    /* I/O console at 7C00
     *  SCCACMD at 0x7C02
     *  SCCADTA at 0x7C03
     */

    // flag inch_avail
    // counter outch_busy init 0

    // on read from SCCACMD if inch_avail set bit 0. If outch_busy=0 set bit 2.
    // if outch_busy!=0, increment it module 4 (ie, it counts up to 4 then stops at 0)
    //
    // on read from SCCADTA expect inch_avail to be true else fatal error. Return char.
    //
    // on write to SCCADTA expect outch_busy=0 else fatal error, increment outch_busy (to 1)

    // need to mimic the hardware that is controlled like this:
    // CODE KEY    \ -- c    get char from serial port
    //    6 # ( D) PSHS,   BEGIN,   SCCACMD LDB,   1 # ANDB,  NE UNTIL,
    //    SCCADTA LDB,   CLRA,   NEXT ;C
    //
    // CODE KEY?   \ -- f    return true if char waiting
    //    6 # ( D) PSHS,   CLRA,   SCCACMD LDB,   1 # ANDB,
    //    NE IF,   -1 # LDB,   THEN,   NEXT ;C
    //
    // CODE EMIT   \ c --    output character to serial port
    //    BEGIN,   SCCACMD LDA,   4 # ANDA,   NE UNTIL,
    //    SCCADTA STB,   6 # ( D) PULS,   NEXT ;C

    smii_console = console_create();
    smii_console->class_ptr->read = smii_console_read;
    smii_console->class_ptr->write = smii_console_write;

    device_define ( smii_console, 0,
                    0x7C00, BUS_MAP_SIZE, MAP_READWRITE );
}


struct machine smii_machine =
{
	.name = "smii",
	.fault = fault,
	.init = smii_init,
	.periodic = 0,
};


/********************************************************************
 * The Multicomp 6809 machine, a platform for Dragon BASIC.
 * This version has 1 serial port, 56K RAM and 8K ROM
 * The serial port is in a "hole" at 0xFFD0/0xFFD1
 * See:
 * Grant Searle http://searle.hostei.com/grant/Multicomp/index.html
 *
 ********************************************************************/

// console input is blocking.
U8 multicomp09_console_read (struct hw_device *dev, unsigned long addr)
{
    unsigned char ch;
    switch (addr)
    {
    case 00:
        // status bit
        return 0xff;
    case 01:
        ch = getchar();
        if (ch == 127) return 8; // rubout->backspace
        if (ch == 10) return 13; // LF->CR
        return ch;
    default:
        printf("In console_read with addr=0x%08x\n", addr);
        return 0x42;
    }
}

void multicomp09_console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    switch (addr)
	{
        case 00:
            printf("In console_write with addr=0x%08x val=0x%02x\n",addr, val);
            break;

        case 01:
            putchar(val);
            break;

        default:
            printf("In console_write with addr=0x%08x val=0x%02x\n",addr, val);
        }
}


void multicomp09_init (const char *boot_rom_file)
{
    struct hw_device *multicomp09_console;
    struct hw_device *iodev;
    struct hw_device *romdev;
    int i;

    /* RAM from 0 to DFFF */
    device_define ( ram_create (0xE000), 0,
                    0x0000, 0xE000, MAP_READWRITE );

    // use the defn below when working on the NXM core-dump bug.
    //    device_define ( ram_create (0x8000), 0,
    //                    0x0000, 0x8000, MAP_READWRITE );

    /* ROM from E000 to FFFF */
    romdev = rom_create (boot_rom_file, 0x2000);
    device_define (romdev, 0,
                   0xE000, 0x2000, MAP_READABLE);

    /* I/O console */
    multicomp09_console = console_create();
    multicomp09_console->class_ptr->read = multicomp09_console_read;
    multicomp09_console->class_ptr->write = multicomp09_console_write;

    /* I/O console at FFD0/FFD1
       Use an ioexpand device at 0xFF80-0xFFFF.
       This overlays the ROM mapping and creating it after
       the ROM overwrites the ROM's mapping.
       The ioexpand provides 16 slots of 8 bytes
       Use some for the IO, map the others back to the ROM
       In particular, need locations 0xFFF0-0xFFFF mapped
       in order to provide the exception vectors.
     */
    iodev = ioexpand_create();
    device_define(iodev, 0, 0xFF80, 128, MAP_READWRITE);
    for (i=0; i<16; i++) {
        if (i==10) {
            // 0xFFD0-0xFFD7
            ioexpand_attach(iodev, i, 0, multicomp09_console);
        }
        else {
            // Map to ROM. Read will have an address of 0x0-0xf
            // so we need to apply an offset to get it to the
            // right place. The offset is from the start of the
            // romdev and has nothing to do with the actual
            // bus address of the location.
            ioexpand_attach(iodev, i, 0x1F80 + (i*8), romdev);
        }
    }
}




struct machine multicomp09_machine =
{
	.name = "multicomp09",
	.fault = fault,
	.init = multicomp09_init,
	.periodic = 0,
};

