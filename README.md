# uacs-lynx: Tools for dynamic analysis of dynamic code

## Overview
The tools in this repository support analysis of application-level instruction execution traces.  In particular, they focus on analysis of *dynamic code*, i.e., code that can be created or modified at  runtime.

The core components of the toolset are a *trace writer* and a *trace reader*.  Other analyses are built using these components.  Documentation for each of these tools is available in the directory containing the code for the tool.

## Core components

### Trace writer

The trace writer monitors the execution of an application and writes an instruction-level execution trace of the client application into a file.  Writing instruction-level execution traces into files can reult in large trace files and high I/O costs.  However, it offers a number of advantages as well, including the flexibility of off-line execution playback and analysis that can be decoupled from the original execution and thus need not be performed on the same execution platform.

The current implementation of the trace writer is availabile in the directory `tracer`.  It uses Intel's [Pin](https://software.intel.com/content/www/us/en/develop/articles/pin-a-dynamic-binary-instrumentation-tool.html) for dynamic instrumentation and recording.

### Trace reader

The trace reader reads the instructions recorded in a trace file and provides them to various analysis tools.  Most analysis tools rely primariy on the reader; the tracer's function is simply to provide instruction traces for analysis.

The current implementation of the trace reader is availabile in the directory `reader`.  It uses Intel's [XED](https://intelxed.github.io/) for disassembly.  A tutorial on how to use it is available in the file `reader/HOW-TO-USE.md`.

## Other analysis tools

Other analysis tools built using the trace writer and reader described above include:

- `alloc_chk` : records and checks heap allocations.
- `cfg` : constructs *dynamic control flow graphs*, which extend the traditional notion of contriol flow graphs to dynamic code, i.e., code that can be created or modified at runtime.
- `funcall-trace` : analyzes function calls and returns in an execution trace.
- `slice` : constructs backward dynamic slices of dynamic code.
- `taint` : a taint propagation library.
- `trace2ascii` : reads an instruction-level trace and writes out the trace as ASCII text.

