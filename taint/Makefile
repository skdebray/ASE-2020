DEST = libtaint.a

# This make file assumes that the environment variable XED_ROOT points to the
# root of the directory tree containing the xed disassembler

CXX = g++
CC = gcc
CFILES =  ../shared/LynxReg.c ByteMemoryTaint.c ByteRegTaint.c Taint.c BytePropagation.c ConstPropagation.c
CXXFILES = LabelStore.cpp
OFILES = $(CFILES:c=o) $(CXXFILES:cpp=o)
CFLAGS = -Wall -g -c
CXXFLAGS = -Wall -g -fpic -c -std=c++11
INCLUDES = -I$(XED_ROOT)/obj -I$(XED_ROOT)/include/public/xed -I../shared -I../reader
LDFLAGS = -shared -g -L$(XED_ROOT)/obj -L../reader
LIBS = -lxed -ltrd

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

$(DEST) : $(OFILES)
	ar rcs $(DEST) $(OFILES)

.PHONY: clean
clean:
	rm -f ../shared/*.o *.o $(DEST)
