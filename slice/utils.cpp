/*
 * File: utils.cpp
 * Purpose: Various utility routines
 */

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <cassert>
#include <unordered_set>
#include <algorithm>
#include <utility>
#include <iostream>
#include "slice.h"
#include "sliceState.h"
#include <libgen.h>
#include <xed-interface.h>
#include <xed-iclass-enum.h>

using namespace std;
using std::vector;
using std::unordered_set;

extern "C" {
    #include "../shared/LynxReg.h"
    #include <Reader.h>
    #include <Taint.h>
    #include <cfg.h>
    #include <cfgAPI.h>
    #include <cfgState.h>
    #include <controlTransfer.h>
}


void printUsage(char *program) {
    printf("Usage: %s [OPTIONS] trace_file 0xSlicingAddress\n", program);
    printf("  OPTIONS:\n");
    printf("    -a addr : slice backwards starting from address addr (in base 16)\n");
    printf("    -b fn_name : begin the trace at function fn_name\n");
    printf("    -c : compress the block size in the dot file\n");
    printf("    -e fn_name : end the trace at function fn_name\n");
    printf("    -f fn_name : trace only the function fn_name\n");
    printf("    -h : print usage\n");
    printf("    -i trace_file : read the instruction trace from file trace_file\n");
    printf("    -n num : slice backwards starting at position num in the trace\n");
    printf("    -o fname : generate output into the file fname; implies -p\n");
    printf("    -p : print the slice (printed to stdout if -o is not specified)\n");
    printf("    -r : keep registers involved in taint calculations when loading from mem\n");
    printf("    -s : generate cfg only from the given sources\n");
    printf("    -S : make CFG with superblocks\n");
    printf("    -t t_id : trace only the thread with id t_id (in base 10)\n");
    printf("    -V : print slice validation info\n");
}


void parseCommandLine(int argc, char *argv[], SlicedriverState *driver_state) {
  if (argc < 3) {
    printUsage(argv[0]);
    exit(1);
  }

  int i;
  char *endptr;
  
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0) {        /* address to slice from */
      i++;
      driver_state->sliceAddr = strtoull(argv[i], &endptr, 16);
      if (*endptr != '\0') {
	fprintf(stderr,
		"WARNING: target address %s contains unexpected characters: %s\n",
		argv[i],
		endptr);
      }
    }
    else if (strcmp("-b", argv[i]) == 0) {
      driver_state->beginFn = argv[++i];
    }
    else if (strcmp("-c", argv[i]) == 0) {
      driver_state->compress = true;
    }
    else if (strcmp("-e", argv[i]) == 0) {
      driver_state->endFn = argv[++i];
    }
    else if (strcmp("-f", argv[i]) == 0) {
      driver_state->traceFn = argv[++i];
    }
    else if (strcmp("-h", argv[i]) == 0) {
      printUsage(argv[0]);
      exit(0);
    }
    else if (strcmp("-i", argv[i]) == 0) {
      driver_state->trace = argv[++i];        /* input trace file */
    }
    else if (strcmp("-n", argv[i]) == 0) {
      i++;
      driver_state->slice_pos = strtoull(argv[i], &endptr, 0);    /* position to slice from */
    }
    else if (strcmp("-o", argv[i]) == 0) {
      driver_state->print_slice = true;
      driver_state->outfile = argv[++i];

      driver_state->outf = fopen(driver_state->outfile, "w");
      if (driver_state->outf == NULL) {
	perror(driver_state->outfile);
	exit(1);
      }
    }
    else if (strcmp("-p", argv[i]) == 0) {
      driver_state->print_slice = true;
    }
    else if (strcmp("-r", argv[i]) == 0) {
      driver_state->keepReg = 1;
    }
    else if (strcmp("-s", argv[i]) == 0) {
      driver_state->includeSrcs.push_back(++i);
    }
    else if (strcmp("-S", argv[i]) == 0) {
      driver_state->makeSuperblocks = true;
    }
    else if (strcmp("-t", argv[i]) == 0) {
      driver_state->targetTid = strtoul(argv[++i], NULL, 10);
    }
    else if (strcmp("-V", argv[i]) == 0) {
      driver_state->validate = true;
    }
    else {
      fprintf(stderr, "Unrecognized command line argument [ignoring]: %s\n", argv[i]);
    }
  }

  return;
}

/*
 * print_slice_instrs() -- print out the instructions in the backward dynamic slice
 */
void print_slice_instrs(SliceState *slice, SlicedriverState *driver_state) {
  std::set<cfgInstruction *>::iterator iter;
  cfgInstruction *cfg_ins;
  ReaderEvent rd_event;
  ReaderIns instr;
  xed_decoded_inst_t xedIns;
  xed_machine_mode_enum_t mmode;
  xed_address_width_enum_t stack_addr_width;
  xed_error_enum_t xed_err;
  char mnemonic[256];
  
  if (!driver_state->print_slice) {
    return;
  }

  // initialize the XED tables -- one time.
  xed_tables_init();
  mmode=XED_MACHINE_MODE_LONG_64;
  stack_addr_width = XED_ADDRESS_WIDTH_64b;

  for (iter = slice->saveableCFGInstructions.begin();
       iter != slice->saveableCFGInstructions.end();
       ++iter) {
    cfg_ins = *iter;
    rd_event = (*iter)->event;
    assert(rd_event.type != EXCEPTION_EVENT);
    instr = rd_event.ins;

    xed_decoded_inst_zero(&xedIns);
    xed_decoded_inst_set_mode(&xedIns, mmode, stack_addr_width);
    xed_err = xed_decode(&xedIns, instr.binary, instr.binSize);
    assert(xed_err == XED_ERROR_NONE);

    getInsMnemonic(&xedIns, mnemonic, 256);
    fprintf(driver_state->outf, "[ph: %d]; %s;  %lx;    %s;\n",
	    cfg_ins->phaseID, cfg_ins->block->fun->name, instr.addr, mnemonic);
    
  }

  fclose(driver_state->outf);
}

