DEST = slicer

TOOLDIR = ..
XEDPATH = ../xed

CXX = g++

INCLUDES = -I$(TOOLDIR)/reader \
	-I$(TOOLDIR)/shared \
	-I$(XEDPATH)/include/public/xed \
	-I$(XEDPATH)/obj -I$(TOOLDIR)/cfg \
	-I$(TOOLDIR)/taint
LDFLAGS = -L$(TOOLDIR)/reader -L$(XEDPATH)/obj -L$(TOOLDIR)/cfg -L$(TOOLDIR)/taint
LIBS = -ltrd -lcfg -lxed -ltaint

CXXFLAGS = -std=c++11 -Wall -g -DDEBUG -O2

CXXFILES = slice.cpp driver.cpp utils.cpp
OFILES = $(CXXFILES:.cpp=.o)

%.o : %.cpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $<

$(DEST) : $(OFILES)
	$(CXX) $(CXXFLAGS) $(OFILES) -o $(DEST) $(LDFLAGS) $(LIBS)

.PHONY: clean
clean:
	/bin/rm -f *.o *.BAK *~ $(OFILES) $(DEST)

