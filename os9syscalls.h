/********************************************************************
* os9.h - NitrOS-9 System Definitions
*
* $Id$
*
* Edt/Rev  YYYY/MM/DD  Modified by
* Comment
* ------------------------------------------------------------------
*/
/*               ORG       0 */
char *os9syscall[] = {
"F$Link",       /* $00 Link to Module */
"F$Load",       /* $01 Load Module from File */
"F$UnLink",     /* $02 Unlink Module */
"F$Fork",       /* $03 Start New Process */
"F$Wait",       /* $04 Wait for Child Process to Die */
"F$Chain",      /* $05 Chain Process to New Module */
"F$Exit",       /* $06 Terminate Process */
"F$Mem",        /* $07 Set Memory Size */
"F$Send",       /* $08 Send Signal to Process */
"F$Icpt",       /* $09 Set Signal Intercept */
"F$Sleep",      /* $0A Suspend Process */
"F$SSpd",       /* $0B Suspend Process */
"F$ID",         /* $0C Return Process ID */
"F$SPrior",     /* $0D Set Process Priority */
"F$SSWI",       /* $0E Set Software Interrupt */
"F$PErr",       /* $0F Print Error */
"F$PrsNam",     /* $10 Parse Pathlist Name */
"F$CmpNam",     /* $11 Compare Two Names */
"F$SchBit",     /* $12 Search Bit Map */
"F$AllBit",     /* $13 Allocate in Bit Map */
"F$DelBit",     /* $14 Deallocate in Bit Map */
"F$Time",       /* $15 Get Current Time */
"F$STime",      /* $16 Set Current Time */
"F$CRC",        /* $17 Generate CRC ($17) */

/*               IFGT      Level-1  */

        /* NitrOS-9 Level 2 system calls */
"F$GPrDsc",     /* $18 Get Process Descriptor copy ($18) */
"F$GBlkMp",     /* $19 Get System Block Map copy ($19) */
"F$GModDr",     /* $1A Get Module Directory copy ($1A) */
"F$CpyMem",     /* $1B Copy External Memory ($1B) */
"F$SUser",      /* $1C Set User ID number ($1C) */
"F$UnLoad",     /* $1D Unlink Module by name ($1D) */
"F$Alarm",      /* $1E Color Computer 3 Alarm Call ($1E) */
"F$1F",         /* $1F RMB       1                   Reserved - For overlap of other systems ($1F) */
"F$20",         /* $20 RMB       1                   Reserved - For overlap of other systems ($1F) */
"F$NMLink",     /* $21 Color Computer 3 Non-Mapping Link ($21) */
"F$NMLoad",     /* $22 Color Computer 3 Non-Mapping Load ($22) */

/*               ELSE

* NitrOS-9 Level 1 system call padding
               RMB       11

               ENDC
*/

"F$Debu",       /* $23 Drop the system into the debugger ($23) */
"F$24",         /* $24 Dummy */

/*               IFGT      Level-1

               ORG       $25 */
"F$TPS",        /* $25 Return System's Ticks Per Second */
"F$TimAlm",     /* $26 CoCo individual process alarm call */

/*               ENDC  */

             /*  ORG       $27                 Beginning of System Reserved Calls */
/* NitrOS-9 common system calls */
"F$VIRQ",    /* $27 Install/Delete Virtual IRQ */
"F$SRqMem",  /* $28 System Memory Request */
"F$SRtMem",  /* $29 System Memory Return */
"F$IRQ",     /* $2A Enter IRQ Polling Table */
"F$IOQu",    /* $2B Enter I/O Queue */
"F$AProc",   /* $2C Enter Active Process Queue */
"F$NProc",   /* $2D Start Next Process */
"F$VModul",  /* $2E Validate Module */
"F$Find64",  /* $2f Find Process/Path Descriptor */
"F$All64",   /* $30 Allocate Process/Path Descriptor */
"F$Ret64",   /* $31 Return Process/Path Descriptor */
"F$SSvc",    /* $32 Service Request Table Initialization */
"F$IODel",   /* $33 Delete I/O Module */

           /*    IFGT      Level-1  */

"F$SLink",   /* $34  System Link */
"F$Boot",    /* $35 Bootstrap System */
"F$BtMem",   /* $36 Bootstrap Memory Request */
"F$GProcP",  /* $37 Get Process ptr */
"F$Move",    /* $38 Move Data (low bound first) */
"F$AllRAM",  /* $39 Allocate RAM blocks */
"F$AllImg",  /* $3A Allocate Image RAM blocks */
"F$DelImg",  /* $3B Deallocate Image RAM blocks */
"F$SetImg",  /* $3C Set Process DAT Image */
"F$FreeLB",  /* $3D Get Free Low Block */
"F$FreeHB",  /* $3E Get Free High Block */
"F$AllTsk",  /* $3F Allocate Process Task number */
"F$DelTsk",  /* $40 Deallocate Process Task number */
"F$SetTsk",  /* $41 Set Process Task DAT registers */
"F$ResTsk",  /* $42 Reserve Task number */
"F$RelTsk",  /* $43 Release Task number */
"F$DATLog",  /* $44 Convert DAT Block/Offset to Logical */
"F$DATTmp",  /* $45 Make temporary DAT image (Obsolete) */
"F$LDAXY",   /* $46 Load A [X,[Y]] */
"F$LDAXYP",  /* $47 Load A [X+,[Y]] */
"F$LDDDXY",  /* $48 Load D [D+X,[Y]] */
"F$LDABX",   /* $49 Load A from 0,X in task B */
"F$STABX",   /* $4A Store A at 0,X in task B */
"F$AllPrc",  /* $4B Allocate Process Descriptor */
"F$DelPrc",  /* $4C Deallocate Process Descriptor */
"F$ELink",   /* $4D Link using Module Directory Entry */
"F$FModul",  /* $4E Find Module Directory Entry */
"F$MapBlk",  /* $4F Map Specific Block */
"F$ClrBlk",  /* $50 Clear Specific Block */
"F$DelRAM",  /* $51 Deallocate RAM blocks */
"F$GCMDir",  /* $52 Pack module directory */
"F$AlHRAM",  /* $53 Allocate HIGH RAM Blocks */

/* Alan DeKok additions */
"F$ReBoot",  /* $54 Reboot machine (reload OS9Boot) or drop to RSDOS */
"F$CRCMod",  /* $55 CRC mode, toggle or report current status */
"F$XTime",   /* $56 Get Extended time packet from RTC (fractions of second) */
"F$VBlock",  /* $57 Verify modules in a block of memory, add to module directory */
"F$58",
"F$59",
"F$5A",
"F$5B",
"F$5C",
"F$5D",
"F$5E",
"F$5F",
"F$60",
"F$61",
"F$62",
"F$63",
"F$64",
"F$65",
"F$66",
"F$67",
"F$68",
"F$69",
"F$6A",
"F$6B",
"F$6C",
"F$6D",
"F$6E",
"F$6F",

           /*    ENDC
*
* Numbers $70 through $7F are reserved for user definitions
*
               ORG       $70
               IFEQ      Level-1
               RMB       16                  Reserved for user definition
               ELSE        */

"F$RegDmp",  /* $70 Ron Lammardo's debugging register dump */
"F$NVRAM",   /* $71 Non Volatile RAM (RTC battery backed static) read/write */
"F$72",
"F$73",
"F$74",
"F$75",
"F$76",
"F$77",
"F$78",
"F$79",
"F$7A",
"F$7B",
"F$7C",
"F$7D",
"F$7E",
"F$7F",


/**************************************
* I/O Service Request Code Definitions
*/
/*             ORG       $80  */
"I$Attach",  /* $80 Attach I/O Device */
"I$Detach",  /* $81 Detach I/O Device */
"I$Dup",     /* $82 Duplicate Path */
"I$Create",  /* $83 Create New File */
"I$Open",    /* $84 Open Existing File */
"I$MakDir",  /* $85 Make Directory File */
"I$ChgDir",  /* $86 Change Default Directory */
"I$Delete",  /* $87 Delete File */
"I$Seek",    /* $88 Change Current Position */
"I$Read",    /* $89 Read Data */
"I$Write",   /* $8A Write Data */
"I$ReadLn",  /* $8B Read Line of ASCII Data */
"I$WritLn",  /* $8C Write Line of ASCII Data */
"I$GetStt",  /* $8D Get Path Status */
"I$SetStt",  /* $8E Set Path Status */
"I$Close",   /* $8F Close Path */
"I$DeletX",  /* $90 Delete from current exec dir */
};
