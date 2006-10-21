#
# This makefile builds m6809-run, a simulator for m6809 processors under Linux.
# It also contains targets for building and running some sample applications.
#

# Host-side configuration
HOST_CC = gcc
HOST_CFLAGS += -I.

ifeq ($(profile),y)
HOST_CFLAGS += -pg
HOST_LFLAGS += -pg
endif
HOST_CFLAGS += -O2

# Target-side configuration
TARGET_SIM = m6809-run
TARGET_BINDIR = /usr/local/m6809/bin
TARGET_INCLUDEDIR = ../../newlib6809/trunk/newlib/libc/include
TARGET_CC = $(TARGET_BINDIR)/gcc
TARGET_CFLAGS = -fno-builtin -save-temps -I$(progdir) -I$(TARGET_INCLUDEDIR)
TARGET_LDFLAGS=-Xlinker --section-start -Xlinker vector=0xFFF0

# Enable verbose option in ld, to see what it's really doing
ifdef VERBOSE
TARGET_LDFLAGS += -Xlinker --verbose
endif

# Enable interactive debugging
ifdef DEBUG
HOST_CFLAGS += -DDEBUG_MONITOR
endif

progdir=prog
ifdef prog
.PHONY : $(prog)
$(prog) : $(progdir)/$(prog)

.PHONY : run
run : $(TARGET_SIM) $(prog)
	@./$(TARGET_SIM) $(progdir)/$(prog)

.PHONY : runall
runall : $(TARGET_SIM) $(PROGLIST)

endif

TARGET_OBJS = $(progdir)/$(prog).o

EXEC_OBJS = 6809.o main.o monitor.o gdb.o

.PHONY : build
build : $(TARGET_SIM)

.PHONY : install
install : build
	@echo "Installing simulator..." && $(SUDO) cp -p m6809-run /usr/local/bin

$(TARGET_SIM) : $(EXEC_OBJS)
	@echo "Linking simulator..." && $(HOST_CC) -o $@ $(EXEC_OBJS) $(HOST_LFLAGS)

$(EXEC_OBJS) : CC=$(HOST_CC) 
$(EXEC_OBJS) : CFLAGS=$(HOST_CFLAGS)

$(TARGET_OBJS) : CC=$(TARGET_CC) 
$(TARGET_OBJS) : CFLAGS=$(TARGET_CFLAGS)

$(progdir)/$(prog): $(TARGET_OBJS) $(EXTRA_$(prog)_OBJS)
	$(TARGET_CC) $(TARGET_LDFLAGS) -o $@ $(TARGET_OBJS)
	
clean:
	@echo "Cleaning all objects..." && rm -f $(EXEC_OBJS) $(TARGET_OBJS) *.i *.s $(progdir)/*.i $(progdir)/*.s $(progdir)/*.o $(progdir)/*.s19 $(progdir)/*.map $(TARGET_SIM)
