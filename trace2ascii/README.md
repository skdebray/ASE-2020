# trace2ascii

This program prints out a textual representation of a trace.

## Usage

    text2ascii [OPTIONS] trace_file

## Options
    -a addr  : print only the instruction at address addr
    -b fname : begin the trace at the function fname
    -e fname : end the trace at the function fname
    -f fname : trace only the function fname
    -t t_id  : trace only the thread t_id
    -h : print usage

## Output
The output produced by `trace2ascii` lists the instructions (and any exception events) in the input trace file, in their occurrence order in the trace, one instruction per line.  Each line consists of a sequence of `;`-delimited fields in the following order:

    position in the trace (start = 0);
    thread id;
    instruction address;
    file the instruction originated in;
    function the instruction belongs to;
    binary encoding of the instruction;
    instruction mnemonic;
    register and memory values;

