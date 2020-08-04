# This make file can be used to create both statically-linked and
# dynamically-linked versions of the library; by default, it creates
# a statically-linked one.  Alternatively, the appropriate one can
# be created by specifying the particular make file to use:
#
# for a statically-linked library: use Makefile-static
# for a dynamically-linked library: use Makefile-dynamic

#libcfg.a : a statically-linked version of the CFG library
libcfg.a :
	make clean; /bin/rm -f libcfg.a; make -f Makefile-static

# libcfg.so : a dynamically-linked version of the CFG library
libcfg.so :
	make clean; /bin/rm -f libcfg.so; make -f Makefile-dynamic

.PHONY: all clean realclean
all : libcfg.so libcfg.a

clean:
	/bin/rm -f *.BAK *~ *.o

realclean:
	/bin/rm -f *.BAK *~ *.o libcfg.so libcfg.a
