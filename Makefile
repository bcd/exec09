
HOST_CC = gcc
HOST_CFLAGS += -I.
TARGET_CC = /usr/local/m6809/bin/gcc
TARGET_CFLAGS = -fomit-frame-pointer -fno-builtin -save-temps -I$(progdir)
TARGET_LDFLAGS=-Xlinker --sectionstart -Xlinker vector=0xFFF0
# TARGET_LDFLAGS += -Xlinker --verbose

ifndef prog
prog=hello
endif
progdir=prog

TARGET_OBJS = $(progdir)/$(prog).o $(progdir)/libsim.o
EXEC_OBJS = 6809.o main.o monitor.o

run : exec09 $(prog)
	@./exec09 $(progdir)/$(prog)

exec09 : $(EXEC_OBJS)
	@echo "Linking simulator..." && $(HOST_CC) -o $@ $(EXEC_OBJS)

$(EXEC_OBJS) : CC=$(HOST_CC) 
$(EXEC_OBJS) : CFLAGS=$(HOST_CFLAGS)

$(TARGET_OBJS) : CC=$(TARGET_CC) 
$(TARGET_OBJS) : CFLAGS=$(TARGET_CFLAGS)

.PHONY : $(prog)
$(prog) : $(progdir)/$(prog)

$(progdir)/$(prog): $(TARGET_OBJS)
	@echo "Linking target program..." && $(TARGET_CC) $(TARGET_LDFLAGS) -o $@ $(TARGET_OBJS)
	
clean:
	@echo "Cleaing all objects..." && rm -f $(EXEC_OBJS) $(TARGET_OBJS) *.i *.s $(progdir)/*.i $(progdir)/*.s
