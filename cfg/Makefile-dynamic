TOOLDIR = ..

# set XEDPATH to the directory containing XED
XEDPATH = ../../xed

CC = gcc

IFLAGS = -I$(TOOLDIR)/reader -I$(TOOLDIR)/shared -I$(XEDPATH)/include/public/xed -I$(XEDPATH)/obj -I$(TOOLDIR)/cfg -I$(TOOLDIR)/taint
LFLAGS = -L$(TOOLDIR)/reader -L$(XEDPATH)/obj -I$(TOOLDIR)/cfg -L$(TOOLDIR)/taint

CFLAGS = -Wall -g -DDEBUG -fPIC
HASH = 

HFILES = cfgAPI.h hashtable.h function.h utils.h cfg.h controlTransfer.h block.h edge.h dot.h
CFILES = cfgAPI.c hashtable.c cfg.c controlTransfer.c block.c edge.c function.c utils.c dot.c

OFILES = $(CFILES:.c=.o)

libcfg.so : $(OFILES)
	$(CC) -shared $(CFLAGS) $(LFLAGS) -o $@ $^ -ltrd -lxed

%.o : %.c
	$(CC) -c $(CFLAGS) $(IFLAGS) -o $@ $<

cfg : cfgDriver.c
	$(CC) -I. $(IFLAGS)  -g -Wall -o $@ $^ $(LFLAGS) -L. $(LFLAGS) -lcfg -ltrd -lxed

cfgRE : cfgRE.c
	$(CC) -I. $(IFLAGS)  -g -Wall -o $@ $^ $(LFLAGS) -L. $(LFLAGS) -lcfg -ltrd -lxed

insCounter : insCounter.c
	$(CC) -I. $(IFLAGS)  -g -Wall -o $@ $^ $(LFLAGS) -L. $(LFLAGS) -lcfg -ltrd -lxed

hashtable.o : hashtable.c hashtable.h cfg.h

controlTransfer.o : controlTransfer.c controlTransfer.h cfg.h

dot.o : dot.c dot.h utils.h

utils.o : utils.c utils.h cfg.h

block.o : block.c block.h cfg.h

cfgAPI.o : cfgAPI.c cfgAPI.h cfg.h

edge.o : cfg.h edge.c edge.h

cfg.o : cfg.c cfg.h

function.o : function.c function.h cfg.h

cfgAPI.o : cfgAPI.c cfgAPI.h cfg.h

cfg.so : cfg.c cfg.h
	$(CC) -shared $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	/bin/rm -f *.o cfg libcfg.so
