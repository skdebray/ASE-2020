# Using the Taint Library: A Tutorial

This document provides a high-level introduction to using the taint library.  It is not intended to be a detailed and exhaustive exposition of the taint library API; for this, please see the README file and the source code.

## Accessing the taint library
Accessing the taint library requires the following:

1) indicate where the compiler can find the relevant include files;
2) indicate which libraries the compiler should link against; and
3) indicate to the dynamic linker where to search for dynamically linked libraries.

Let `TOOLDIR` be the path to the directory containing the `taint` directory (i.e., the parent of this directory) and `XEDPATH` be the path to the directory containing Intel's XED disassembler.

- **Include files:** The include path for various header files mentioned below is given by:

  ```
  INCLUDES = -I$(TOOLDIR)/reader \
	-I$(TOOLDIR)/shared \
	-I$(TOOLDIR)/taint \
	-I$(XEDPATH)/include/public/xed \
	-I$(XEDPATH)/obj
	```

- **Libraries:** The reader uses the XED disassembler as a dynamically linked library `libxed.so`, which should be in the file `$(XEDFILE)/obj/libxed.so`.  For performance reasons, the reader is by default compiled into a statically linked library `libtrd.a` (however, if desired it can be compiled into a dynamically linked library using the make file `Makefile-dynamic`).  To specify the libraries, use

  ```
  LDFLAGS = -L$(TOOLDIR)/reader -L$(TOOLDIR)/taint -L$(XEDPATH)/obj
  LIBS = -ltrd -ltaint -lxed
  ```

- **Dynamic loader search path:** Set the variable `LD_LIBRARY_PATH` as follows:
    - `LD_LIBRARY_PATH` should include the directory `XEDPATH/obj`;
    - if the reader is built as a dynamically linked library, i.e., as a `*.so` file, then `LD_LIBRARY_PATH` should also include the directory `TOOLDIR/reader` (the default build process for the reader builds it as a statically shared library, i.e., as a `*.a` file, and it is not necessary to have the path to the reader in `LD_LIBRARY_PATH`);
    - if the taint library is built as a dynamically linked library, i.e., as a `*.so` file, then `LD_LIBRARY_PATH` should also include the directory `TOOLDIR/taint` (the default build process for the taint library builds it as a statically shared library, i.e., as a `*.a` file, and it is not necessary to have the path to the taint library in `LD_LIBRARY_PATH`).

Given the above, a makefile for a client application that uses the taint library might look something like the following:

```
TOOLDIR = .....        # set appropriately
XEDPATH = .....        # set appropriately

INCLUDES = -I$(TOOLDIR)/reader \
	-I$(TOOLDIR)/taint \
	-I$(TOOLDIR)/shared \
	-I$(XEDPATH)/include/public/xed \
	-I$(XEDPATH)/obj 
LDFLAGS = -L$(TOOLDIR)/reader -L$(TOOLDIR)/taint -L$(XEDPATH)/obj
LIBS = -ltrd -ltaint -lxed 

CC = gcc
CFLAGS = -Wall -g -O2

CFILES = .....
OFILES = $(CFILES:.c=.o)

%.o : %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c $<

$(TARGET) : $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o $(TARGET) $(LDFLAGS) $(LIBS)

```

## Taint analysis: Overview
Taint analysis maps locations (registers and memory) to *taint labels*.  The taint library described here represents taint labels using 64-bit unsigned integer (`uint64_t`) values.  This allows a large number of distinct taint labels and makes it possible to carry out fine-grained taint analysis if desired.  The current implementation of the library performs byte-level taint analysis.

The high-level structure of a taint processing application is:

- initialize trace reader and taint state;
- for each event in the input trace file:
  - introduce, remove, and/or print taint if appropriate;
  - process the event;
- cleanup

## Processing an execution trace
To propagate taint through an instruction trace, we have to repeatedly read in execution events (i.e., instructions) from the trace and then propagate trace through each event read in.  Details on how to do this are given in the tutorial on the trace reader (`reader/HOW-TO-USE.md`).  This document describes the additional steps needed for taint propagation.

### Taint state initialization
Taint state should be initialized before processing a trace:

``` C
#include <Taint.h>
...
ReaderState *r_state = initReader(trace_file, 0);
...
TaintState *t_state = initTaint(r_state);
```
Among other things, this registers a set of handler functions that implement the *taint policy*, i.e., the logic by which the taint analysis determines which locations to taint at which point.  The `initTaint()` function mentioned above uses a set of handlers that implement byte-level taint tracking; if a different taint policy is desired, the user can either write a different initialization function, or else redefine selected handler functions.

### Taint label creation
Given a taint state `t_state`, a new taint label can be created as follows:

``` C
uint64_t taint_label = getNewLabel(t_state);
```

### Taint introduction
Taint can be introduced into a given taint state `t_state` as follows.

- To apply the taint label `lbl` to a register `reg` in thread `tid` or a memory block of size `sz` starting at address `start_addr`:

  ``` C
  taintReg(t_state, reg, tid, lbl);
  taintMem(t_state, start_addr, sz, lbl);
  ```
  The argument `reg` is of type `LynxReg` (defined in the file `$TOOLDIR/shared/LynxReg.h` -- see above for TOOLDIR).  This sets the taint labels for the locations specified to lbl, overwriting any previous taint at those locations.

- To apply the taint label `lbl` to a single operand `op` or a collection of operands `op_list` containing `num_ops` operands (e.g., all source operands, or all destination operands), in thread `tid`:
  ``` C
  taintOperand(t_state, op, tid, lbl, ins_info);
  taintOperandList(t_state, op_list, num_ops, tid, lbl, ins_info);
  ```
  Here `ins_info` is the `InsInfo` structure for the instruction that the operand `op` belongs to (see the tutorial on the trace reader for more information about `InsInfo`).

### Taint retrieval and output

Given a taint state `t_state`, the taint values for registers and memory locations can be output as follows:

- To print out the tainted registers and memory locations together with their taint labels:

  ``` C
  outputTaint(t_state);
  ```

The taint library currently provides mostly relatively low-level interfaces for more focused taint retrieval: see the functions `getByteMemTaint()` and `getByteRegTaint()` in the code.  **This needs to be fixed.**

### Taint propagation

Given a taint state `t_state`, we can propagate taint through an instruction as follows:

- **Forward propagation:**  propagate taint forward through a reader event (i.e., instruction) `r_event`, with corresponding `InsInfo` structure `ins_info` (see documentation on the trace reader for more about `ReaderEvent` and `InsInfo` structures):

  ``` C
  propagateForward(t_state, r_event, ins_info, ins_flag);
  ```
  Here `ins_flag` is a flag that indicates how registers involved in address calculations (i.e., base and index registers) should be handled: taint from such registers is propagated into the results of the computation only if this flag is not 0.

- **Backward propagation:** propagate taint backward through an instruction with `InsInfo` structure `ins_info and thread `tid`:

  ``` C
  propagateBackward(t_state, tid, ins_info);
  ```





