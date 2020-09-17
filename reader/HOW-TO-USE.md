# Using the Trace Reader: A Tutorial

This document provides a high-level introduction to the trace reader.  It is not intended to be a detailed and exhaustive exposition of the trace reader API; for this, please see the README file and the source code.

## Accessing the trace reader
Accessing the trace reader requires the following:

1) indicate where the compiler can find the relevant include files;
2) indicate which libraries the compiler should link against; and
3) indicate to the dynamic linker where to search for dynamically linked libraries.

Let `TOOLDIR` be the path to the directory containing the `reader` directory (i.e., the parent of this directory) and `XEDPATH` be the path to the directory containing Intel's XED disassembler.

- **Include files:** The include path for various header files mentioned below is given by:

  ```
  INCLUDES = -I$(TOOLDIR)/reader \
	-I$(TOOLDIR)/shared \
	-I$(XEDPATH)/include/public/xed \
	-I$(XEDPATH)/obj
	```

- **Libraries:** The reader uses the XED disassembler as a dynamically linked library `libxed.so`, which should be in the file `$(XEDFILE)/obj/libxed.so`.  For performance reasons, the reader is by default compiled into a statically linked library `libtrd.a` (however, if desired it can be compiled into a dynamically linked library using the make file `Makefile-dynamic`).  To specify the libraries, use

  ```
  LDFLAGS = -L$(TOOLDIR)/reader -L$(XEDPATH)/obj
  LIBS = -ltrd -lxed
  ```

- **Dynamic loader search path:** Set the variable `LD_LIBRARY_PATH` as follows:
    - `LD_LIBRARY_PATH` should include the directory `XEDPATH/obj`;
    - if the reader is built as a dynamically linked library, i.e., as a `*.so` file, then `LD_LIBRARY_PATH` should also include the directory `TOOLDIR/reader` (the default build process for the reader builds it as a statically shared library, i.e., as a `*.a` file, and it is not necessary to have the path to the reader in `LD_LIBRARY_PATH`).

Given the above, a makefile for a client application that uses the trace reader might look something like the following:

```
TOOLDIR = .....
XEDPATH = .....

INCLUDES = -I$(TOOLDIR)/reader \
	-I$(TOOLDIR)/shared \
	-I$(XEDPATH)/include/public/xed \
	-I$(XEDPATH)/obj 
LDFLAGS = -L$(TOOLDIR)/reader -L$(XEDPATH)/obj
LIBS = -ltrd -lxed 

CC = gcc
CFLAGS = -Wall -g -O2

CFILES = .....
OFILES = $(CFILES:.c=.o)

%.o : %.c
	$(CC) $(INCLUDES) $(CFLAGS) -c $<

$(TARGET) : $(OFILES)
	$(CC) $(CFLAGS) $(OFILES) -o $(TARGET) $(LDFLAGS) $(LIBS)

```

## Trace files
A trace file is a binary file that consists of a header followed by a sequence of *events*.  Each event represents either a normal instruction execution or else an exception.  The reader provides information about each event to the client application using the reader.

## Processing an execution trace
To process an execution trace, we first initialize the reader state, then repeatedly fetch and process execution events.  In most cases, the client application will need to disassemble and analyze the instructions that are executed.  It can do this using Intel's [XED](https://intelxed.github.io/) disassembler.  The code to do this has the structure shown below.  The types `ReaderState` and `ReaderEvent` are defined in the file `reader/Reader.h`.  It is the client's responsibility to allocate space for a `ReaderEvent` structure and pass a pointer to it to `nextEvent()`, which fills in its fields.


``` C
#include <XedDisassembler.h>
#include <Reader.h>

void process_trace() {
  ReaderState *reader_state;
  ReaderEvent curr_event;
  InsInfo ins_info;
  xed_machine_mode_enum_t mmode;
  xed_address_width_enum_t stack_addr_width;

  char *trace_file = ... ;    /* name of trace file */
  
  /* initialize XED tables */
  xed_tables_init();
  mmode = XED_MACHINE_MODE_LONG_64;
  stack_addr_width = XED_ADDRESS_WIDTH_64b;

  /* initialize reader state */
  reader_state = initReader(trace_file, 0);

  /* process the trace */
  while (nextEvent(reader_state, &curr_event)) {
    if (curr_event.type == INS_EVENT) {
       ...  /* normal execution */
    }
    else if (curr_event.type == EXCEPTION_EVENT) {
       ...  /* exception */
    }
    else {
      ...   /* unrecognized event type */
    }
  }    /* while */
  
  closeReader(reader_state);
}
```
The value `XED_ADDRESS_WIDTH_64b` indicates that stack addresses are 64 bits wide; for 32-bit addresses use `XED_ADDRESS_WIDTH_32b`.


## Accessing information about the execution
The trace reader maintains information about the program's execution state as execution progresses.  This information is updated at each execution event, and can be accessed by the client analyses.

Information about an execution can be divided into the following categories:

1) Global summary information about the execution.
2) The execution state of the program (memory and register values).
3) Properties of each executed instruction.

We discuss each of these categories in the following sections.

### Global summary information

The total number of threads created during an execution can be obtained using

``` C
uint32_t getNumThreads(ReaderState *state)
```

### Memory and register values

The trace reader maintains a shadow architecture to track memory and register values through execution; see files `ShadowMemory.[ch]` and `ShadowRegisters.[ch]`.  Register and memory values can be accessed using

``` C
const uint8_t *getRegisterVal(ReaderState *state, LynxReg reg, uint32_t thread);
void getMemoryVal(ReaderState *state, uint64_t addr, uint32_t size, uint8_t *buf);
```

Here, `LynxReg` refers to an enumeration of x86-64 registers used by this toolset.  It is defined in the file `.../shared/LynxReg.h`.

**Note:**

1) `getRegisterVal()` returns a string representation of the value of the register. This can be converted to a binary representation using `strtoul()` if necessary.
2) `getMemoryVal()` expects the buffer `buf` to be large enough to store the value of the memory region specified; this is not checked.
3) For each event in a trace, the shadow architecture provides the execution state *immediately before* that event.  To obtain information about the execution state after an event, use the state associated with (i.e., just before) the next event in the trace.

### Properties of instructions
To improve space efficiency, information about instructions is split into two data structures, `InsInfo` and `ReaderIns`, that are defined in the file `Reader.h`.

- `InsInfo` contains information that is invariant across all occurrences of an instruction in a program.  Examples include: the source and destination operands, the textual representation of the instruction, etc.
- `ReaderIns` contains information that may be different for different occurrences of an instruction in a program.  Examples include: the thread id of a dynamic instance of an instruction; the binary encoding of the instruction; its address in memory; (information about) the function it belongs to; etc.

The `InsInfo` data structure is not updated automatically when `nextEvent()` is called (this is to make it possible to access the program's execution state after an event).  It is processed as follows (see the README file for details):

- **Initialization**:
  `void initInsInfo(InsInfo *info)`
- **Update**:
  `int fetchInsInfo(ReaderState *state, ReaderIns *ins, InsInfo *info)`

For example, client code to print out the address and mnemonic of each executed instruction would be something like the following (code shown earlier, e.g., to initialize XED, is omited to reduce clutter):

``` C
  InsInfo info;
  ReaderState reader_state = initReader(trace_file, 0);    /* initialize reader state */
  initInsInfo(&info);    /* initialize InsInfo */

  /* process the trace */
  while (nextEvent(reader_state, &curr_event)) {             /* update reader state */
    if (curr_event.type == INS_EVENT) {
      fetchInsInfo(reader_state, &curr_event.ins, &info);    /* update InsInfo */
      printf("ADDR: 0x%lx  INSTR: %s\n", curr_event.ins.addr, info.mnemonic);
    }
  }
```

### Instruction operands

Information about the operands of an instruction is available in its `InsInfo` structure.  

#### Operand categories.
Operands fall into three categories: *source operands*, which are read by the instruction; *destination operands*, which are written by the instruction; and *read-write operands*, which are both read and written.  For any given instruction, the `InsInfo` structure provides the following information for each category of operand (see the definition of `InsInfo` in the file `Reader.h` for specifics):

1) the number of operands of that category; and
2) a collection of operands that can be iterated over.

The following code shows how the destination operands of an instruction may be accessed.  In this code, the variable `i` is just a counter to keep track of the number of operands processed.

``` C
  InsInfo *info = ...;
  ReaderOp *op;
  ...
  op = info->dstOps;    /* first destination operand */
  for (int i = 0; i < info->dstOpCnt; i++) {
    ... process op ...
    
    op = op->next;      /* next destination operand */
  }
```

The handling of source and read-write operands is similar.

#### Operand types
The type of an operand `op` is given by `op.type`.  Its possible values are given by

``` C
typedef enum {
    NONE_OP,
    REG_OP,
    MEM_OP,
    UNSIGNED_IMM_OP,
    SIGNED_IMM_OP
} ReaderOpType;
```

Information about an operand is stored in a `ReaderOp` structure:

``` C
typedef struct ReaderOp_t {
    uint8_t mark;
    ReaderOpType type;    // the type of this operand 
    union {
        LynxReg reg;      // see the file LynxReg.h
        ReaderMemOp mem;
        uint64_t unsignedImm;
        int64_t signedImm;
    };
    struct ReaderOp_t *next;        // Pointer to the next ReaderOp
} ReaderOp;

```

#### Operand values
Given a reader state `r_state` and an instruction with thread-id `tid`, the value of an operand `op` for the instruction can be obtained as follows:

- **Register operands:** `op` is a register operand if `op.type == REG_OP`; in this case, the register is given by `op.reg`.  Suppose that this register is *r*, then the value of the register can be obtained using

  <code>getRegisterVal(r_state, *r*, tid)</code>.
  
  Note that the function `getRegisterVal()` returns a pointer to a base-16 string representation of the value of the register. If desired, this string can be converted to a binary value using `strtoul`.
- **Memory operands:** `op` is a memory operand if `op.type == MEM_OP`; in this case, the address and size (no. of bytes) of the memory locations referenced are given by `op.mem.addr` and `op.mem.size` respectively.  The contents of this memory region can be obtained into a buffer `buf` of appropriate size using

  `getMemoryVal(r_state, op.mem.addr, op.mem.size, buf);`

- **Immediate operands:** The 64-bit value of a signed immediate operand can be obtained as `op.signedImm`.  The 64-bit value of an unsigned immediate operand can be obtained as `op.unsignedImm`.

