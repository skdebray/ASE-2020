
XED_DIR=../xed

CC=gcc
CFLAGS=-g
INCLUDES=-I$(XED_DIR)/include/public/xed -I$(XED_DIR)/obj -I../reader -I../shared
LIBPATHS=-L$(XED_DIR)/obj -L../reader
LIBS=-ltrd -lxed

trace2ascii : trace2ascii.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBPATHS) $(LIBS)

clean :
	rm -rf trace2ascii
