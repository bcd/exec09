#include <fcntl.h>
#include <assert.h>
#include "machine.h"
#include "6809.h"
#include "ioexpand.h"
#include "command.h"

// for smii console
int smii_i_avail = 1;
int smii_o_busy = 0;

// for multicomp09 Rx FIFO
#define CharFIFO_SIZE (8)
// for multicomp09 sdmapper: values most-recently written to these registers
#define MULTICOMP09_RAMMAX (0x80000)
// to match RTL, FRT_BLOCK must be 7
#define FRT_BLOCK (7)
unsigned char mc_mmuadr = 0x00;
unsigned char mc_mmufrt = 0x00; // Magic flag for Fixed RAM Top
unsigned char mc_mmudat = 0x00;
unsigned char mc_timer  = 0x00;
unsigned char mc_pblk[16]; // [7] is protect, [6:0] is physical block
unsigned char mc_sdlba2; // because we're doing maths on them
unsigned char mc_sdlba1;
unsigned char mc_sdlba0;
char mc_data[512];
int  mc_addr;
int  mc_state;
int  mc_poll;
int  mc_dindex;
struct CharFIFO {
    int wr;
    int rd;
    int empty;
    char data[CharFIFO_SIZE];
};

static struct CharFIFO uart0_rx_fifo;
static unsigned char uart0_wr_status;

// multicomp devices
struct hw_device *mc_romdev, *mc_ramdev, *mc_iodev, *mc_consoledev, *mc_sdmapperdev;

FILE *sd_file;
FILE *batch_file;
FILE *log_file;
FILE *dump_file;

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
    switch (addr) {
    case 0x02: // SCCACMD
        // on output make it seem busy for several polls
        smii_o_busy = smii_o_busy == 0 ? 0 : (smii_o_busy + 1)%4;
        //        printf("02 smii_o_busy = %d return 0x%02x\n",smii_o_busy,(smii_i_avail & 1) | (smii_o_busy == 0 ? 4 : 0));
        return (smii_i_avail & 1) | (smii_o_busy == 0 ? 4 : 0);
    case 0x03: // SCCADTA
        if (batch_file && fread( &ch, 1, 1, batch_file)) {
        }
        else {
            ch = getchar();
        }
        // key conversions to make keyboard look DOS-like
        if (ch == 127) ch = 8; //backspace
        if (ch == 10) ch = 13; //cr
        return ch;
    default:
        printf("In smii_console_read with addr=0x%08x\n", (unsigned int)addr);
        return 0x42;
    }
}


void smii_console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    switch (addr) {
    case 0x00:
    case 0x02:
        // UART setup. Not emulated; just ignore it.
        break;
    case 0x03: // SCCADTA
        if (smii_o_busy != 0) printf("Oops! Write to busy UART\n");
        smii_o_busy = 1;
        putchar(val);
        break;
    default:
        printf("In smii_console_write with addr=0x%08x val=0x%02x\n",(unsigned int)addr, val);
    }
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
       no further use of it. Since the model is strict, the ROM
       is read-only and the write causes a trap.

       To avoid the trap, create 1-page dummy devices at each
       location in order to silently ignore the writes.

       For locations that alias to ROM, change the attribute so
       that writes are ignored.
    */
    quiet = null_create();
    device_define(quiet, 0,    0x8000, BUS_MAP_SIZE, MAP_IGNOREWRITE);
    device_define(quiet, 0,    0x9000, BUS_MAP_SIZE, MAP_IGNOREWRITE);
    device_define(quiet, 0,    0xA000, BUS_MAP_SIZE, MAP_IGNOREWRITE);
    device_define(quiet, 0,    0xB000, BUS_MAP_SIZE, MAP_IGNOREWRITE);
    device_define(quiet, 0,    0xC000, BUS_MAP_SIZE, MAP_IGNOREWRITE);
    device_define(quiet, 0,    0xD000, BUS_MAP_SIZE, MAP_IGNOREWRITE);

    device_define(rom, 0x0000, 0xE000, BUS_MAP_SIZE, MAP_IGNOREWRITE | MAP_READABLE);
    device_define(rom, 0x1000, 0xF000, BUS_MAP_SIZE, MAP_IGNOREWRITE | MAP_READABLE);


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



    /* If a file smii.bat exists, supply input from it until
       it's exhausted.
    */
    batch_file = file_open(NULL, "smii.bat", "rb");
}


struct machine smii_machine =
{
	.name = "smii",
	.fault = fault,
	.init = smii_init
};


/********************************************************************
 * The Multicomp 6809 machine
 * This version has 3 serial ports, 56K RAM and 8K ROM, SDCARD i/f.
 * GPIO, memory-mapper and timer interrupt.
 * The I/O is all at 0xFFD0-0xFFDF
 * See:
 * Grant Searle http://searle.hostei.com/grant/Multicomp/index.html
 * and:
 * https://www.retrobrewcomputers.org/doku.php?id=boards:sbc:multicomp:cycloneii-c:start
 *
 ********************************************************************/

/* UART-style console.
   Previously, the input FIFO was not modelled:
   Data input is sort-of non-blocking: if you read the UART status
   bit and it says RX Char Available, you can read the data register
   and know that it will not stall.
   However, if you read the data register and no character is
   available (rude!) the input will block until you press a key.
   Could have it work other than this.. reading the data register
   could also call kbhit. When no character is available could
   return 0xff or somesuch, which would more accurately model real behaviour.

   Now, the FIFO is modelled and so are UART Rx interrupts:
   1/ each read of status or data calls kbhit and puts a char
   (if any) into the FIFO and then:
   1a/ print error message if FIFO overflow
   1b/ for status: char available = FIFO-not-empty
   1c/ for data: if FIFO-not-empty, pop and return oldest char else
   print error message (application error) and return 0xff
   2/ register an .update routine for the console_class. The update
   routine is called in the exec09 main loop on a periodic basis.
   3/ the update routine calls kbbit and puts a char (if any) into
   the FIFO and then:
   3a/ print error message if FIFO overflow
   3b/ if UART interrupt enabled and FIFO-not-empty: call IRQ
   4/ IRQ ISR needs to understand that a UART IRQ arrived rather
   than a timer IRQ. TBD how to handle that.. probably by modelling
   the timer interrupt in a more complex way.. eg, by testing the mask
   *or* by implementing a new mask bit for the UART IRQ.
   5/ on status write of 3 (reset), clear the FIFO.

   The result should work correctly for existing applications:
   - polled I/O in FLEX and CamelForth and Cubix and MSBASIC
   - timer-driven I/O in FUZIX (and can change FUZIX to allow
     rx interrupts, if it does not already)
   - lastly, it should allow NitrOS9 L1 to do terminal input
     and therefore boot to the command prompt.

   offset 0,1 - 1st UART - virtual UART. Main console
   offset 2,3 - 2nd UART
   offset 4,5 - 3rd UART
   offset 6,7 - GPIO unit
 */
U8 multicomp_console_rxpoll (void)
{
    unsigned char ch;
    if (batch_file) {
        // do not allow the batch file to create an overflow
        if (uart0_rx_fifo.empty || (uart0_rx_fifo.rd != uart0_rx_fifo.wr)) {
            fread( &ch, 1, 1, batch_file);
            uart0_rx_fifo.empty = 0;
            uart0_rx_fifo.data[uart0_rx_fifo.wr] = ch;
            uart0_rx_fifo.wr = (uart0_rx_fifo.wr + 1) % CharFIFO_SIZE;
        }
    }
    else {
        if (kbhit()) {
            ch = kbchar();
            if ((uart0_rx_fifo.wr == uart0_rx_fifo.rd) && (uart0_rx_fifo.empty == 0)) {
                // discard newest char - the real hardware is not polite
                // like this! Should really generate a receiver overrun.
                printf("[uart0 error: rx FIFO overflow]\n");
            }
            else {
                uart0_rx_fifo.empty = 0;
                uart0_rx_fifo.data[uart0_rx_fifo.wr] = ch;
                uart0_rx_fifo.wr = (uart0_rx_fifo.wr + 1) % CharFIFO_SIZE;
            }
        }
    }
}


U8 multicomp09_console_read (struct hw_device *dev, unsigned long addr)
{
    //printf("In console_read with addr=0x%08x pc=0x%04x\n", (unsigned int)addr, get_pc());
    unsigned char ch;
    switch (addr) {

    case 0: /* UART0 status */
        // b7: interrupt
        // b1: can accept Tx char
        // b0: Rx char available
        multicomp_console_rxpoll();
        if  (uart0_rx_fifo.empty) {
            return 2;
        }
        else {
            return (uart0_wr_status & 0x80) | 3;
        }

    case 1: /* UART0 data */
        multicomp_console_rxpoll();
        if  (uart0_rx_fifo.empty) {
            printf("[uart0 error: read from EMPTY data register]\n");
            return 0xff;
        }
        else {
            ch = uart0_rx_fifo.data[uart0_rx_fifo.rd];
            uart0_rx_fifo.rd = (uart0_rx_fifo.rd + 1) % CharFIFO_SIZE;
            if (uart0_rx_fifo.rd == uart0_rx_fifo.wr) {
                uart0_rx_fifo.empty = 1;
                release_irq(1);
            }
            // key conversions to make keyboard look DOS-like
            if (ch == 127) return 8; // rubout->backspace
            if (ch == 10) return 13; // LF->CR
            return ch;
        }

    case 2: /* UART1 status */
        // hardware supports bits [7], [1], [0]
        printf("[uart1 stat rd addr=0x%08x]\n", (unsigned int)addr);
        return 0x03;

    case 3: /* UART1 data */
        printf("[uart1 data rd addr=0x%08x]\n", (unsigned int)addr);
        return 0x42;

    case 4: /* UART2 status */
        // hardware supports bits [7], [1], [0]
        printf("[uart2 stat rd addr=0x%08x]\n", (unsigned int)addr);
        return 0x03;

    case 5: /* UART2 data */
        printf("[uart2 data rd addr=0x%08x]\n", (unsigned int)addr);
        return 0x42;

        /* GPIO (not modelled) ---------------*/
    case 6:
    case 7:
        printf("[gpio rd addr=0x%08x]\n", (unsigned int)addr);
        return 0x42;

        /* Other (should be unreachable) -----*/
    default:
        printf("ERROR in console_read with addr=0x%04x\n", (unsigned int)addr);
        return 0x42;
    }
}

void multicomp09_console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    //printf("In console_write with addr=0x%08x val=0x%02x pc=0x%04x\n", (unsigned int)addr, val, get_pc());
    //fprintf(log_file,"%02x~%02x\n",(unsigned char)(addr&0xff),val);
    switch (addr) {

    case 0: /* UART0 status */
        uart0_wr_status = val;
        if (val==3) { /* master reset */
            uart0_rx_fifo.wr=0;
            uart0_rx_fifo.wr=0;
            uart0_rx_fifo.empty=1;
            uart0_wr_status = 0;
            release_irq(1);
            printf("[uart0 reset]");
        }
        else {
            printf("[uart0 stat wr: PC=0x%04x, addr=0x%04x, wdata=0x%02x]\n", get_pc(), (unsigned int)addr, val);
        }
        break;

    case 1: /* UART0 data */
        putchar(val);
        fflush(stdout);
        break;

    case 2: /* UART1 status */
        if (val==3) {
            printf("[uart1 reset]");
        }
        else {
            printf("[uart1 stat wr: PC=0x%04x, addr=0x%04x, wdata=0x%02x]\n", get_pc(), (unsigned int)addr, val);
        }
        break;

    case 3: /* UART1 data */
        printf("[uart1 data wr of 0x%02x]\n", val);
        break;

    case 4: /* UART2 status */
        if (val==3) {
            printf("[uart2 reset]");
        }
        else {
            printf("[uart2 stat wr: PC=0x%04x, addr=0x%04x, wdata=0x%02x]\n", get_pc(), (unsigned int)addr, val);
        }
        break;

    case 5: /* UART2 data */
        printf("[uart2 data wr of 0x%02x]\n", val);
        break;

        /* GPIO (not modelled) ---------------*/
    case 6:
    case 7:
        printf("[gpio wr addr=0x%04x val=0x%02x]\n", (unsigned int)addr, val);
        break;

        /* Other (should be unreachable) -----*/
    default:
        printf("ERROR in console_write with addr=0x%04x val=0x%02x\n",(unsigned int)addr, val);
    }
}

void multicomp09_console_update (struct hw_device *dev)
{
    multicomp_console_rxpoll();
    if (uart0_rx_fifo.empty == 0 && (uart0_wr_status & 0x80)) {
        request_irq(1);
    }
}

// given a physical block number (0-7) return the offset into physical memory
int mmu_offset(int blk) {
    int tmp;
    int idx;
    if (mc_mmuadr & 0x20) {
        // mmu enabled
        // 0-7 if tr=0, 8-15 if tr=1
        idx = blk | ((mc_mmuadr & 0x40) >> 3);
        tmp = mc_pblk[idx];
        return (tmp & 0x7f) << 13;
    }
    else {
        return blk << 13;
    }
}

// given a physical block number (0-7) return the flags
int mmu_flags(int blk) {
    int tmp;
    int idx;
    if (mc_mmuadr & 0x20) {
        // mmu enabled
        // 0-7 if tr=0, 8-15 if tr=1
        idx = blk | ((mc_mmuadr & 0x40) >> 3);
        tmp = mc_pblk[idx];
        return (tmp >> 7) == 1 ? MAP_READABLE : MAP_READWRITE;
    }
    else {
        return MAP_READWRITE;
    }
}


/* SDCARD and memory mapper
   FFDF SDCARD MMUDAT  wo
   FFDE SDCARD MMUADR  wo
   FFDD SDCARD TIMER   rw
   FFDC SDCARD SDLBA2  wo
   FFDB SDCARD SDLBA1  wo
   FFDA SDCARD SDLBA0  wo
   FFD9 SDCARD SDCONTROL/SDSTATUS
   FFD8 SDCARD SDDATA r/w

   Access using 32-bit address in which the low 9 bits are 0 (each sector is 512 bytes). The
   address is set using the SDLBA registers like this:

    31 30 29 28.27 26 25 24.23 22 21 20.19 18 17 16.15 14 13 12.11 10 09 08.07 06 05 04.03 02 01 00
   +------- SDLBA2 -----+------- SDLBA1 --------+------- SDLBA0 --------+ 0  0  0  0  0  0  0  0  0

   SDSTATUS (RO)
   b7     Write Data Byte can be accepted
   b6     Read Data Byte available
   b5     Block Busy
   b4     Init Busy
   b3     Unused. Read 0
   b2     Unused. Read 0
   b1     Unused. Read 0
   b0     Unused. Read 0

   SDCONTROL (WO)
   b7:0   0x00 Read block
          0x01 Write block


   mc_state = 0 no card. Will never complete initialisation
              1 initialising. Will initialise after 16 polls
              of status register (hokey but works!)
              2 idle. Read and Write commands allowed.
              3 in read. Takes 3 polls for each byte becomes
              available.
              4 in write. Takes 3 polls before each byte
              can be accepted.
   error if too much data written or read or if command while
   not idle.
 */


/*
  op values are:
  0 remap memory using existing register values
  1 load new dat value then remap memory using existing register values
  2 load new adr value then remap memory using new values
*/
// mc_mmuadr bits:
// 7 ROMDIS
// 6 TR
// 5 MMUEN
// 4 NMI
// 3:0 MAPSEL
//
void sdmapper_remap(int op, int val)
{
    int i;

    // Update appropriate mapping register if dat written
    if (op == 1) {
        mc_pblk[mc_mmuadr & 0xf] = val;
        if (val > 0x3f) {
            printf("Bad: write to mmu map register 0x%01x with data 0x%02x at pc=0x%04x\n",0xf & mc_mmuadr, val, get_pc());
        }

        // If the MMU is enabled and the data register that changed is part of the active mapping, report the mapping
        if ( ((mc_mmuadr & 0x68) == 0x68) || ((mc_mmuadr & 0x68) == 0x20) ) {
            fprintf(log_file,"INFO mmudat change on active mapping set: mmudat 0x%02x->0x%02x\n",mc_mmudat, val);
            fprintf(log_file,"INFO mmuadr 0x%02x       pc=0x%04x", mc_mmuadr, get_pc());
            for (i=0;i<16;i++) {
                fprintf(log_file," %02d:0x%02x",i,mc_pblk[i]);
            }
            fprintf(log_file,"\n");
            fflush(NULL);
        }

        mc_mmudat = val;
    }


    if (op == 2) {
        if ( ((mc_mmuadr & 0xa0) == 0xa0) && ((val & 0xa0) == 0x20) ) {
            // ROMDIS=1 and MMUEn=1 and wrote ROMDIS=0 and MMUEn=1 - cue to
            // set FRT bit and IGNORE transition of ROMDIS
            mc_mmufrt = 1;
            val = val | 0x80;
            fprintf(log_file,"INFO mmuadr 0x%02x->0x%02x frt=%1x pc=0x%04x\n", mc_mmuadr, val, mc_mmufrt, get_pc());
        }
        if ( (val & 0x20) == 0x00 ) {
            // MMUEn is disabled - cue to clear FRT bit
            mc_mmufrt = 0;
            fprintf(log_file,"INFO mmuadr 0x%02x->0x%02x frt=%1x pc=0x%04x\n", mc_mmuadr, val, mc_mmufrt, get_pc());
        }

        // If ROMDIS, TR or MMUEN have changed, report the mapping
        if ((mc_mmuadr & 0x70) != (val & 0x70)) {
            fprintf(log_file,"INFO mmuadr 0x%02x->0x%02x frt=%1x pc=0x%04x", mc_mmuadr, val, mc_mmufrt, get_pc());
            for (i=0;i<16;i++) {
                fprintf(log_file," %02d:0x%02x",i,mc_pblk[i]);
            }
            fprintf(log_file,"\n");
            fflush(NULL);
        }

        mc_mmuadr = val;
    }


    // now update mapping based on mc_mmuadr, mc_pblk[], mc_mmufrt

    //[NAC HACK 2015May06] Yeuch. Horrible how I have had to hardwire the device numbers.
    //overall, it would be better if I could pick them in the first place.
    // 0 null-device
    // 1 RAM
    // 2 ROM
    // 3 CONSOLE
    // 4 SDMAPPER
    // 5 IOEXPAND


    //      addr    dev  offset         len     flags

    bus_map(0x0000, 1,   mmu_offset(0), 0x2000, mmu_flags(0)); // block 0
    bus_map(0x2000, 1,   mmu_offset(1), 0x2000, mmu_flags(1)); // block 1
    bus_map(0x4000, 1,   mmu_offset(2), 0x2000, mmu_flags(2)); // ..
    bus_map(0x6000, 1,   mmu_offset(3), 0x2000, mmu_flags(3));
    bus_map(0x8000, 1,   mmu_offset(4), 0x2000, mmu_flags(4));
    bus_map(0xA000, 1,   mmu_offset(5), 0x2000, mmu_flags(5));
    bus_map(0xC000, 1,   mmu_offset(6), 0x2000, mmu_flags(6)); // block 6

    // Block 7 is cut up in pieces..

    // 0xE000-0xFDFF (0x1E00 bytes) behaves as ROM or MMappable RAM
    if (mc_mmuadr & 0x80) {
        // ROM device disabled; map RAM.
        bus_map(0xE000, 1,   mmu_offset(7), 0x1E00, mmu_flags(7));
    }
    else {
        // ROM device, starting at offset 0
        bus_map(0xE000, 2,   0x0000,        0x1E00, MAP_READABLE);
    }

    // The next 0x180 bytes is the first part of the Fixed RAM Top
    // which behaves as ROM or MMappable RAM or RAM from a fixed block
    if (mc_mmuadr & 0x80) {
        // ROM device disabled; map 0x180 bytes of RAM
        if (mc_mmufrt) {
            // RAM from fixed block -- always allow read/write
            bus_map(0xFE00, 1,   0x1E00 + (FRT_BLOCK << 13), 0x180, mmu_flags(7));
        }
        else {
            // RAM from block controlled by MMU
            bus_map(0xFE00, 1,   0x1E00 + mmu_offset(7),     0x180, MAP_READWRITE);
        }
    }
    else {
        // ROM device, starting at offset 0x1E00
        bus_map(0xFE00, 2,   0x1E00,        0x180, MAP_READABLE);
    }

    // The final 0x80 bytes at the very top of the address space is broken
    // into 16 devices, each 8 bytes in size. Two of the devices correspond
    // to addresses 0xFFDx -- the I/O devices. The remaining 14 slots act
    // in the same way as Fixed RAM Top and each of them applies the same
    // rules as the other FRT region above.
    bus_map(0xFF80, 5,   0x0000, 128,    MAP_READWRITE); // ioexpand

    for (i=0; i<16; i++) {
        if (i==10) {
            // 0xFFD0-0xFFD7 -- VDU/UART and GPIO
            ioexpand_attach(mc_iodev, i, 0, mc_consoledev);
        }
        else if (i==11) {
            // 0xFFD8-0xFFDF -- SDCARD and MemMapper
            ioexpand_attach(mc_iodev, i, 0, mc_sdmapperdev);
        }
        else {
            // Map to underlying memory. Access will have an address of 0x0-0xf
            // so we need to apply an offset to get it to the right place. The
            // offset is from the start of the underlying device (ROM or RAM)
            // and has nothing to do with the actual bus address of the location.
            // The access permissions are inherited from the underlying device,
            // which is exactly what I want.
            // [NAC HACK 2017Feb01] is the access permission correct for the
            // write-protection flag on the MMU devices??
            if (mc_mmuadr & 0x80) {
                // ROM device disabled; map 0x10 bytes of RAM
                if (mc_mmufrt) {
                    // RAM from fixed block -- always allow read/write
                    ioexpand_attach(mc_iodev, i, (FRT_BLOCK << 13) + 0x1F80 + (i*8), mc_ramdev);
                }
                else {
                    // RAM from block controlled by MMU
                    ioexpand_attach(mc_iodev, i, mmu_offset(7) + 0x1F80 + (i*8), mc_ramdev);
                }
            }
            else {
                // ROM device, starting at offset 0x1F80
                ioexpand_attach(mc_iodev, i, 0x1F80 + (i*8), mc_romdev);
            }
        }
    }
}


U8 sdmapper_read (struct hw_device *dev, unsigned long addr)
{
    //printf("INFO In sdmapper_read with addr=0x%08x, mc_state=%d, mc_poll=%d\n", addr, mc_state, mc_poll);
    switch (addr) {
    case 0: // SDDATA
        switch (mc_state) {
        case 0: case 1: case 2:
            //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xff);
            return 0xff;
        case 3: // in read
            if (mc_poll == 3) {
                mc_poll = 0;
                if (mc_dindex == 511) {
                    mc_state = 2; // final read then back to idle
                }
                if (mc_dindex < 512) {
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),mc_data[mc_dindex]);
                    return mc_data[mc_dindex++];
                }
                else {
                    printf("ERROR attempt to read too much data from sd block\n");
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xff);
                    return 0xff;
                }
            }
            else {
                printf("ERROR attempt to read sd block when data not yet available\n");
                //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xff);
                return 0xff;
            }
        case 4: // in write
            printf("ERROR attempt to read sd data during write command\n");
            //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xff);
            return 0xff;
        }
        break; // unreachable
    case 1: // SDCONTROL
        switch (mc_state)
            {
            case 0: // busy and always will remain so
                //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0x90);
                return 0x90;
            case 1: // count polls and initialise
                mc_poll++;
                if (mc_poll == 16)
                    mc_state = 2;
                //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0x90);
                return 0x90;
            case 2: // idle.
                //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0x80);
                return 0x80;
            case 3: // in read
                if (mc_poll < 3)
                    mc_poll++;
                if (mc_poll == 3) {
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xe0);
                    return 0xe0; // data available
                }
                else {
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xa0);
                    return 0xa0; // still waiting TODO maybe 20
                }
            case 4: // in write
                if (mc_poll < 3)
                    mc_poll++;
                if (mc_poll == 3) {
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0xa0);
                    return 0xa0; // space available
                }
                else {
                    //fprintf(log_file,"%02x<%02x\n",(unsigned char)(addr&0xff),0x20);
                    return 0x20; // still waiting
                }
            }
        break; // unreachable
    case 5: // TIMER read
        return mc_timer;
    default:
        // In general, it's an error to read back anything else. However, in the
        // case of a CLR instruction the CPU does a read "modify" write (but
        // ignores the read data. In this case, it's nice *not* to print this
        // warning message.
        // This is a hack because it only ignores CLR with extended addressing
        // and, worse, it might cause a real problem to be missed because
        // we guess/assume the start position of the previous op-code (would be
        // cleaner if we tracker previous pc).
        if (read8(get_pc()-3) != 0x7f) {
            printf("INFO In sdmapper_read with addr=0x%04x\n", (unsigned int)addr);
        }
    }
    return 0x42; // return default value
}

// TODO expand RAM and implement mapper, protect and rom disable
void sdmapper_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    int retvar;
    //printf("INFO In sdmapper_write with addr=0x%08x, mc_state=%d mc_poll=%d mc_dindex=%d\n", addr, mc_state, mc_poll, mc_dindex);
    //fprintf(log_file,"%02x>%02x\n",(unsigned char)(addr&0xff),val);
    switch (addr) {
    case 0: // SDDATA
        switch (mc_state) {
        case 0: case 1: case 2: case 3:
            printf("ERROR attempt to write to sd data during sd block read\n");
            break;
        case 4: // in write
            if (mc_poll == 3) {
                mc_poll = 0;
                if (mc_dindex < 512) {
                    mc_data[mc_dindex++] = val;
                    if (mc_dindex == 512) {
                        // commit the data
                        if (fseek(sd_file, mc_addr, SEEK_SET)) {
                            printf("ERROR seek to sd offset address 0x%x for write failed\n", mc_addr);
                        }
                        else {
                            retvar = fwrite(mc_data, 512, 1, sd_file);
                            assert(retvar != -1);
                        }
                        mc_state = 2; // back to idle.
                    }
                }
                else {
                    printf("ERROR attempt to write too much data to sd block\n");
                }
            }
            else {
                printf("ERROR attempt to write sd data when space not yet available\n");
            }
            break;
        }
        break;
    case 1: // SDCONTROL
        switch (mc_state) {
        case 0:
            printf("ERROR attempt to write to sd control but no sd image file\n");
            break;
        case 1:
            printf("ERROR attempt to write to sd control but still initialising\n");
            break;
        case 2:
            // form address
            mc_addr = ((0x7f & mc_sdlba2) << 25) | (mc_sdlba1 << 17) | (mc_sdlba0 << 9);
            mc_poll = 0;
            switch (val) {
            case 0: // read command
                mc_dindex = 0;
                mc_state = 3;
                if (fseek(sd_file, mc_addr, SEEK_SET)) {
                    printf("ERROR seek to sd offset address 0x%x for read failed\n", mc_addr);
                    for (mc_dindex = 0; mc_dindex < 512; mc_dindex++) {
                        mc_data[mc_dindex] = 0xff;
                        mc_dindex = 0;
                    }
                }
                else {
                    retvar = fread(mc_data, 512, 1, sd_file);
                    assert(retvar != -1);
                }
                break;
            case 1: // write command
                mc_dindex = 0;
                mc_state = 4;
                break;
            default:
                printf("ERROR unknown sd command 0x%02x\n", val);
                break;
            }
            break;
        case 3:
            printf("ERROR attempt to write to sd control during sd block read\n");
            break;
        case 4:
            printf("ERROR attempt to write to sd control during sd block write\n");
            break;
        }
    case 2: // SDLBA0
        mc_sdlba0 = val;
        break;
    case 3: // SDLBA1
        mc_sdlba1 = val;
        break;
    case 4: // SDLBA2
        mc_sdlba2 = val;
        break;
    case 5: // TIMER write
        // bit [1] is read/write, timer enable
        // bit [7] is read, write-1-to-clear, interrupt.

        // Enable or disable
        mc_timer = (mc_timer & 0xfd) | (val & 2);

        if (val & 0x80) {
            release_irq(0);
            mc_timer &= 0x7f;
        }

        break;
    case 6: // MMUADR
        // ignore writes where bit(4) is set; this is the single-step/nmi control
        // and it forces the other write data to be ignored
        if ((val & 0x10) == 0) {
            sdmapper_remap(2, val);
        }
        break;
    case 7: // MMUDAT
        sdmapper_remap(1, val);
        break;
    }
}

void sdmapper_reset (struct hw_device *dev)
{
}

struct hw_class sdmapper_class =
{
	.name = "sdmapper",
	.readonly = 0,
	.reset = sdmapper_reset,
	.read = sdmapper_read,
	.write = sdmapper_write,
};

struct hw_device* sdmapper_create (void)
{
	return device_attach (&sdmapper_class, BUS_MAP_SIZE, NULL);
}



/* Called periodically when exec09 is invoked with the -I option.
   This can be used to model the hardware of a periodic timer and
   (for example) invoke IRQ or FIQ as required by the hardware.
   For multicomp09, model the timer interrupt: if the timer is enabled,
   generate an IRQ.
*/
void multicomp09_tick (void)
{
    if (mc_timer & 0x02) {
        mc_timer |= 0x80;
        request_irq(0);
    }
}

// [NAC HACK 2015May05] is it legal to have more than one instance of anything?
// eg 2 RAMs? I assumed it was but now I realise IT IS NOT! Yeuch.
// I assume the same restriction also applies to ROMs and RAMs
void multicomp09_init (const char *boot_rom_file)
{
    int i;

    /* Log file
    */
    log_file = file_open(NULL, "multicomp09.log", "w+b");
    fprintf(log_file, "===Log file\n");

    /* RAM is 512Kbytes. With MMU disabled low 64K is mapped linearly
       otherwise, it is mapped in 8K chunks. Each chunk has a separate
       write protect.
    */
    mc_ramdev = ram_create(MULTICOMP09_RAMMAX);

    /* ROM is 8Kbytes. Usually sits from E000 to FFFF but can be disabled
       by writing 0x80 to MMUADR
    */
    mc_romdev = rom_create (boot_rom_file, 0x2000);

    /* I/O Devices - UARTs */
    mc_consoledev = console_create();
    mc_consoledev->class_ptr->read   = multicomp09_console_read;
    mc_consoledev->class_ptr->write  = multicomp09_console_write;
    mc_consoledev->class_ptr->update = multicomp09_console_update;

    /* I/O Devices - SD controller, MMapper, Timer */
    mc_sdmapperdev = sdmapper_create();

    /* Address space for I/O devices at FFD0/FFDF
       Use an ioexpand device at 0xFF80-0xFFFF.
       This overlays the ROM mapping and creating it *after*
       the ROM overwrites the ROM's mapping.
       The ioexpand provides 16 slots of 8 bytes
       Use some for the IO, map the others back to the ROM
       In particular, need locations 0xFFF0-0xFFFF mapped
       in order to provide the exception vectors.
     */
    mc_iodev = ioexpand_create();


    /* Now map all the devices into the address space, in accordance
       with the settings of the memory mapper.
    */
    sdmapper_remap(0, 0);

    /* Initialise/reset the UART0 input FIFO and its status register*/
    uart0_rx_fifo.wr = 0;
    uart0_rx_fifo.rd = 0;
    uart0_rx_fifo.empty = 1;
    uart0_wr_status = 0;
    release_irq(1);

    /* If a file multicomp09.bat exists, supply input from it until
       it's exhausted.
    */
    batch_file = file_open(NULL, "multicomp09.bat", "rb");

    /* If a file multicomp09_sd.img exists, open it. sdcard will
       only transition to initialised state if file exists
    */
    sd_file = file_open(NULL, "multicomp09_sd.img", "r+b");
    mc_poll = 0;
    if (sd_file)
        mc_state = 1;
    else
        mc_state = 0;
}


/* Dump just does a binary dump of the RAM space. Properly it should also dump
   the memory mapper state. The dump is only intended to aid debug/analysis. To
   be used for (eg) persistence there's lots more state that would be needed.
*/
void multicomp09_dump (void)
{
    int i;
    char byte;
    dump_file = file_open(NULL, "multicomp09.dmp", "w+b");
    for (i=0; i<MULTICOMP09_RAMMAX; i=i+1) {
        byte = ram_read(mc_ramdev,i);
        /* inefficient!! */
        fwrite(&byte, 1, 1, dump_file);
    }
    fclose(dump_file);
}

struct machine multicomp09_machine =
{
	.name = "multicomp09",
	.fault = fault,
	.init = multicomp09_init,
        .dump = multicomp09_dump,
        .tick = multicomp09_tick
};


/********************************************************************
 * The kipper1 SBC
 *
 * 32KByte of RAM at $0000
 * 6850 ACIA at      $A000,$A001
 * 16KByte of ROM at $C000
 ********************************************************************/

/* UART-style console. Console input is blocking (but should not be)
 */
U8 kipper1_console_read (struct hw_device *dev, unsigned long addr)
{
    //printf("In console_read with addr=0x%08x pc=0x%04x\n", (unsigned int)addr, get_pc());
    unsigned char ch;
    switch (addr) {
    case 0:
        // status bit
        // hardware supports bits [7], [1], [0]
        return 0x03;
    case 1:
        if (batch_file && fread( &ch, 1, 1, batch_file)) {
        }
        else {
            ch = getchar();
        }
        // key conversions to make keyboard look DOS-like
        if (ch == 127) return 8; // rubout->backspace
        if (ch == 10) return 13; // LF->CR
        return ch;
    default:
        printf("In console_read with addr=0x%08x\n", (unsigned int)addr);
        return 0x42;
    }
}

void kipper1_console_write (struct hw_device *dev, unsigned long addr, U8 val)
{
    //printf("In console_write with addr=0x%08x val=0x%02x pc=0x%04x\n", (unsigned int)addr, val, get_pc());
    //fprintf(log_file,"%02x~%02x\n",(unsigned char)(addr&0xff),val);
    switch (addr) {
    case 0:
        printf("In console_write with addr=0x%08x val=0x%02x\n",(unsigned int)addr, val);
        break;

    case 1:
        //if ((val != 0x0d) && (val != 0x20) && (val != 0x0a) && (val < '0')) {
        //    printf("Char 0x%02x", val);
        //}
        putchar(val);
        break;

    default:
        printf("In console_write with addr=0x%08x val=0x%02x\n",(unsigned int)addr, val);
    }
}


void kipper1_init (const char *boot_rom_file)
{
    struct hw_device *kipper1_console, *rom;

    /* 32K RAM from 0000 to 7FFF */
    device_define ( ram_create (0x8000), 0,
                    0x0000, 0x8000, MAP_READWRITE );

    /* 16K ROM from C000 to FFFF */
    rom = rom_create (boot_rom_file, 0x4000);
    device_define (rom , 0,
                   0xC000, 0x4000, MAP_READABLE);

    /* I/O console at A000
     *  SCCACMD at 0xA000
     *  SCCADTA at 0xA001
     */

    // flag inch_avail
    // counter outch_busy init 0

    // on read from SCCACMD if inch_avail set bit 0. If outch_busy=0 set bit 2.
    // if outch_busy!=0, increment it module 4 (ie, it counts up to 4 then stops at 0)
    //
    // on read from SCCADTA expect inch_avail to be true else fatal error. Return char.
    //
    // on write to SCCADTA expect outch_busy=0 else fatal error, increment outch_busy (to 1)

    kipper1_console = console_create();
    kipper1_console->class_ptr->read = kipper1_console_read;
    kipper1_console->class_ptr->write = kipper1_console_write;

    device_define ( kipper1_console, 0,
                    0xA000, BUS_MAP_SIZE, MAP_READWRITE );



    /* If a file kipper1.bat exists, supply input from it until
       it's exhausted.
    */
    batch_file = file_open(NULL, "kipper1.bat", "rb");
}


struct machine kipper1_machine =
{
	.name = "kipper1",
	.fault = fault,
	.init = kipper1_init
};
