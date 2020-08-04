# This make file can be used to create both statically-linked and
# dynamically-linked versions of the library; by default, it creates
# a statically-linked one.  Alternatively, the appropriate one can
# be created by specifying the particular make file to use:
#
# for a statically-linked library: use Makefile-static
# for a dynamically-linked library: use Makefile-dynamic

#libtaint.a : a statically-linked version of the taint library
libtaint.a :
	make clean; /bin/rm -f libtaint.a; make -f Makefile-static

# libtaint.so : a dynamically-linked version of the taint library
libtaint.so :
	make clean; /bin/rm -f libtaint.so; make -f Makefile-dynamic

.PHONY: all clean realclean
all : libtaint.so libtaint.a

clean:
	/bin/rm -f *.BAK *~ *.o

realclean:
	/bin/rm -f *.BAK *~ *.o libtaint.so libtaint.a
