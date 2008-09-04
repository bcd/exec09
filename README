This is a rewrite of Arto Salmi's 6809 simulator.  Many changes
have been made to it.  This program remains licensed under the
GNU General Public License.

Input Files


Machines

The simulator now has the notion of different 'machines':
which says what types of I/O devices are mapped into the 6809's
address space and how they can be accessed.  Adding support for
a new machine is fairly easy.

There are 3 builtin machine types at present.  The default,
called 'simple', assumes that you have a full 64KB of RAM,
minus some input/output functions mapped at $FF00 (see I/O below).
If you compile a program with gcc6809 with no special linker
option, you'll get an S-record file that is suitable for running
on this machine.  The S-record file will include a vector table
at $FFF0, with a reset vector that points to a _start function,
which will call your main() function.  When main returns,
_start writes to an 'exit register' at $FF01, which the simulator
interprets and causes it to stop.

gcc6809 also has a builtin notion of which addresses are used
for text and data.  The simple machine enforces this and will
"fault" on invalid accesses.

The second machine is 'wpc', and is an emulation of the
Williams Pinball Controller which was the impetus for me
working on the compiler in the first place.

The third machine, still in development, is called 'eon'
(for Eight-O-Nine).  It is similar to simple but has some
more advanced I/O capabilities, like a larger memory space
that can be paged in/out, and a disk emulation for programs
that wants to have persistence.

TODO : Would anyone be interested in a CoCo machine type?


Faults


Debugging

The simulator supports interactive debugging similar to that
provided by 'gdb'.

b <expr>
	Set a breakpoint at the given address.

bl
	List all breakpoints.

c
	Continue running.

d <num>
	Delete a breakpoint/watchpoint.

di <expr>
	Add a display expression.  The value of the expression
	is display anytime the CPU breaks.

h
	Display help.

l <expr>
	List CPU instructions.

me <expr>
	Measure the amount of time that a function named by
	<expr> takes.

n
	Continue until the next instruction is reached.
	If the current instruction is a call, then
	the debugger resumes after control returns.

p <expr>
	Print the value of an expression.  See "Expressions" below.

q
	Quit the simulator.

re
	Reset the CPU/machine.

runfor <expr>
	Continue but break after a certain period of (simulated) time.

s
	Step one CPU instruction.

set <expr>
	Sets the value of an internal variable or target memory.
	See "Expressions" below for details on the syntax.

so <file>
	Run a set of debugger commands from another file.
	The commands may start/stop the CPU.  When the commands
	are finished, control returns to the previous input
	source (the file that called it, or the keyboard.)

sym <file>
	Load a symbol table file.  Currently, the only format
	supported is an aslink map file.

td
	Dump the last 256 instructions that were executed.

wa <expr>
	Add a watchpoint.  The CPU will break when the
	memory given by <expr> is modified.

x <expr>
	Examine target memory at the address given.


-----------------------------------------------------------------

Original README text from Arto:


simple 6809 simulator under GPL licence

NOTE! this software is beta stage, and has bugs.
To compile it you should have 32-bit ANSI C complier.

This simulator is missing all interrupt related SYNC, NMI Etc...

I am currently busy with school, thus this is relased.
if you have guestion or found something funny in my code please mail me.

have fun!

arto salmi	asalmi@ratol.fi

history:

2001-01-28 V1.0  original version
2001-02-15 V1.1  fixed str_scan function, fixed dasm _rel_???? code.
2001-06-19 V1.2  Added changes made by Joze Fabcic:
                 - 6809's memory is initialized to zero
                 - function dasm() had two bugs when using vc6.0 complier:
                   - RDWORD macro used two ++ in same variable
                   - _rel_byte address was wrong by 1.
                - after EXIT command, if invalid instruction is encountered, monitor is activated again
                - default file format is motorola S-record
                - monitor formatting


