# slice

This program computes a backward dynamic slice from a trace.

## Usage

    slicer [OPTIONS]

## Options
    -a addr : slice backwards starting from address addr (in base 16)
    -b fn_name : begin the trace at function fn_name
    -c : compress the block size in the dot file
    -e fn_name : end the trace at function fn_name
    -f fn_name : trace only the function fn_name
    -h : print usage
    -i trace_file : read the instruction trace from file trace_file
    -n num : slice backwards starting at position num in the trace
    -o fname : generate output into the file fname; implies -p
    -p : print the slice (printed to stdout if -o is not specified)
    -r : keep registers involved in taint calculations when loading from mem
    -s : generate cfg only from the given sources
    -S : make CFG with superblocks
    -t t_id : trace only the thread with id t_id (in base 10)
    -V : print slice validation info


If both the address (`-a`) and position (`-n`) to slice from are specified, the slicer checks that the address of the instruction at the position specified via `-n` matches the address specified via `-a`.

## Output
The output produced by `slicer` lists the instructions in the backward dynamic slice one instruction per line.  Each line consists of a sequence of `;`-delimited fields in the following order:

    phase id
    function name
    instruction address;
    instruction mnemonic;

