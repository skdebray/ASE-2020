# This make file assumes that the environment variable XED_ROOT points to the
# root of the directory tree containing the xed disassembler

CC=gcc
CFLAGS=-g
INCLUDES=-I$(XED_ROOT)/include/public/xed -I$(XED_ROOT)/obj -I../reader -I../shared
LIBPATHS=-L$(XED_ROOT)/obj -L../reader
LIBS=-ltrd -lxed

trace2ascii : trace2ascii.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBPATHS) $(LIBS)

clean :
	rm -rf trace2ascii
