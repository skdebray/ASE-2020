# This make file can be used to create both statically-linked and
# dynamically-linked versions of the library; by default, it creates
# a statically-linked one.  Alternatively, the appropriate one can
# be created by specifying the particular make file to use:
#
# for a statically-linked library: use Makefile-static
# for a dynamically-linked library: use Makefile-dynamic

#libtrd.a : a statically-linked version of the reader
libtrd.a :
	make clean; /bin/rm -f libtrd.a; make -f Makefile-static

# libtrd.so : a dynamically-linked version of the reader
libtrd.so :
	make clean; /bin/rm -f libtrd.so; make -f Makefile-dynamic

.PHONY: all clean realclean
all : libtrd.so libtrd.a

clean:
	/bin/rm -f *.BAK *~ *.o

realclean:
	/bin/rm -f *.BAK *~ *.o libtrd.so libtrd.a
