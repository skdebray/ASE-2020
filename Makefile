TARGETS = reader trace2ascii cfg taint slice 

.PHONY: all clean

all : $(TARGETS)
	make -C reader 
	make -C trace2ascii 
	make -C taint 
	make -C cfg 
	make -C slice 

clean : $(TARGETS)
	cd reader; make clean
	cd trace2ascii ; make clean
	cd cfg ; make clean
	cd taint ; make clean
	cd slice ; make clean


