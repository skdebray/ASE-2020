.PHONY: clean

CXX = g++
CC = gcc
XEDPATH = ../../xed
CFILES =  ../shared/LynxReg.c ByteMemoryTaint.c ByteRegTaint.c Taint.c BytePropagation.c ConstPropagation.c
CXXFILES = LabelStore.cpp
OFILES = $(CFILES:c=o) $(CXXFILES:cpp=o)
CFLAGS = -Wall -g -fpic -c
CXXFLAGS = -Wall -g -fpic -c -std=c++11
INCLUDES = -I$(XEDPATH)/obj -I$(XEDPATH)/include/public/xed -I../shared -I../reader
LDFLAGS = -shared -g -L$(XEDPATH)/obj -L../reader
LIBS = -lxed -ltrd

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $<

libtaint.so: $(OFILES)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f ../shared/*.o *.o libtaint.so
