
CC = /usr/local/m6809/bin/gcc
CFLAGS = -fomit-frame-pointer -fno-builtin -save-temps -I.

LDFLAGS=-Xlinker --sectionstart -Xlinker vector=0xFFF0
# LDFLAGS += -Xlinker --verbose

ALL_OBJS = hello.o libsim.o

hello : hello.o libsim.o
	$(CC) $(LDFLAGS) -o $@ $< libsim.o

clean:
	rm -f $(ALL_OBJS)
