# Representing and Reasoning about Dynamic Code

## Overview
The code in this repository is described in the paper

> Jesse Bartels, Jon Stephens, and Saumya Debray. "Reprenting and Reasoning about Dynamic Code".  *Proceedings of the 35th IEEE/ACM International Conference on Automated Software Engineering* (ASE 2020), Sept. 2020.

The data files for that paper are available in the directory `https://www2.cs.arizona.edu/projects/lynx-project/Samples/ASE-2020/`.

The tools in this repository support analysis of application-level instruction execution traces.  In particular, they focus on analysis of *dynamic code*, i.e., code that can be created or modified at  runtime.

The core components of the toolset are a *trace writer* and a *trace reader*.  Other analyses are built using these components.  Documentation for each of these tools is available in the directory containing the code for the tool.

## Core components

### Trace writer

The trace writer monitors the execution of an application and writes an instruction-level execution trace of the client application into a file.  Writing instruction-level execution traces into files can reult in large trace files and high I/O costs.  However, it offers a number of advantages as well, including the flexibility of off-line execution playback and analysis that can be decoupled from the original execution and thus need not be performed on the same execution platform.

The current implementation of the trace writer is availabile in the directory `tracer`.  It uses Intel's [Pin](https://software.intel.com/content/www/us/en/develop/articles/pin-a-dynamic-binary-instrumentation-tool.html) for dynamic instrumentation and recording.

### Trace reader

The trace reader reads the instructions recorded in a trace file and provides them to various analysis tools.  Most analysis tools rely primariy on the reader; the tracer's function is simply to provide instruction traces for analysis.

The current implementation of the trace reader is availabile in the directory `reader`.  It uses Intel's [XED](https://intelxed.github.io/) for disassembly.  A tutorial on how to use it is available in the file `reader/HOW-TO-USE.md`.

### DCFG construction
This code, in the directory `cfg`, constructs *dynamic control flow graphs*, which extend the traditional notion of contriol flow graphs to dynamic code, i.e., code that can be created or modified at runtime.

### Slicer
This code, in the directory `slice`, constructs backward dynamic slices of dynamic code.

### Taint
This code, in the directory `taint`, is a taint propagation library.

### Trace2ascii
This tool, in the directory `trace2ascii`, reads an instruction-level trace and writes out the trace as ASCII text.

