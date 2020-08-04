/**
 * This is the driver for the trace2ascii tool. Most of the work is done by the 
 * trace reader (which is linked in as a library). This code just gets instructions
 * one by one and prints out the information to stdout. 
 **/

#include <Reader.h>
#include "stdio.h"

typedef struct {
  char *trace;
  char *beginFn;
  uint32_t beginId;
  char *endFn;
  uint32_t endId;
  char *traceFn;
  uint32_t traceId;
  int64_t targetTid;
  uint64_t target_addr;
  uint8_t hasSrcReg;
  uint8_t hasMemRead;
  uint8_t hasData;
  uint8_t hasSrcId;
  uint8_t hasFnId;
  uint8_t hasAddr;
  uint8_t hasBin;
  uint8_t hasTid;
  uint8_t foundBeginFn;
  uint8_t foundTraceFn;
  uint8_t run;
  uint8_t addrSize;
  uint64_t endStackPtr;
  uint32_t endStackTid;
} Trace2Ascii_state;  

void printUsage(char *program) {
    printf("Usage: %s [OPTIONS] <trace>\n", program);
    printf("Options:\n");
    printf("  -a addr  : print only the instruction at address addr\n");
    printf("  -b fname : begin the trace at the function fname\n");
    printf("  -e fname : end the trace at the function fname\n");
    printf("  -f fname : trace only the function fname\n");
    printf("  -t t_id  : trace only the thread t_id\n");
    printf("  -h : print usage\n");
}

void parseCommandLine(int argc, char *argv[], Trace2Ascii_state *tstate) {
  int i;
  char *endptr;
  /*
   * initialize state
   */
  tstate->trace = NULL;
  tstate->beginFn  = NULL;
  tstate->beginId = -1;
  tstate->endFn = NULL;
  tstate->endId = -1;
  tstate->traceFn = NULL;
  tstate->traceId = -1;
  tstate->targetTid = -1;
  tstate->target_addr = 0;
  /*
   * process command line
   */
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-a") == 0) {
      i++;
      tstate->target_addr = strtoull(argv[i], &endptr, 16);
      if (*endptr != '\0') {
	fprintf(stderr,
		"WARNING: target address %s contains unexpected characters: %s\n",
		argv[i],
		endptr);
      }
    }
    else if (strcmp(argv[i], "-b") == 0) {
      tstate->beginFn = argv[++i];
    }
    else if (strcmp(argv[i], "-e") == 0) {
      tstate->endFn = argv[++i];
    }
    else if (strcmp(argv[i], "-f") == 0) {
      tstate->traceFn = argv[++i];
    }
    else if (strcmp(argv[i], "-t") == 0) {
      tstate->targetTid = strtoul(argv[++i], NULL, 10);
    }
    else if (strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      exit(0);
    }
    else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown option [ignoring]: %s\n", argv[i]);
    }
    else {
      tstate->trace = argv[i];
      break;
    }
  }
  
  return;
}

void printRegOp(ReaderState *readerState, const char *prefix, LynxReg reg, uint32_t tid) {
    reg = LynxReg2FullLynxIA32EReg(reg);
    printf("%s:%s=", prefix, LynxReg2Str(reg));

    const uint8_t *val = getRegisterVal(readerState, reg, tid);

    int i;
    for(i = LynxRegSize(reg) - 1; i >= 0; i--) {
        printf("%02x", val[i]);
    }

    printf(" ");
}

void printMemOp(ReaderState *readerState, const char *prefix, ReaderOp *op, uint32_t tid) {
    if(!op->mem.addrGen) {
        printf("%s[%llx]=", prefix, (unsigned long long) op->mem.addr);
        uint8_t *buf = malloc(sizeof(uint8_t) * op->mem.size);

        getMemoryVal(readerState, op->mem.addr, op->mem.size, buf);

        int i;
        for(i = op->mem.size - 1; i >= 0; i--) {
            printf("%02x", buf[i]);
        }
        printf(" ");

        free(buf);
    }

    if(op->mem.seg != LYNX_INVALID) {
        printRegOp(readerState, "R", op->mem.seg, tid);
    }
    if(op->mem.base != LYNX_INVALID) {
        printRegOp(readerState, "R", op->mem.base, tid);
    }
    if(op->mem.index != LYNX_INVALID) {
        printRegOp(readerState, "R", op->mem.index, tid);
    }
}


/*******************************************************************************
 *                                                                             *
 * chk_trace_state() -- returns 1 if printing of the trace should begin,       *
 * 0 otherwise.                                                                *
 *                                                                             *
 *******************************************************************************/

int chk_trace_state(ReaderEvent *curEvent,
		    ReaderState *readerState,
		    Trace2Ascii_state *tstate) {
  if (tstate->targetTid != -1 && curEvent->ins.tid != tstate->targetTid) {
    return 0;
  }

  if (!tstate->run) {
    if (!tstate->foundBeginFn) {
      if (curEvent->ins.fnId == tstate->beginId) {
	tstate->foundBeginFn = 1;
	tstate->run = (tstate->foundBeginFn && tstate->foundTraceFn);
      }
    }
    if (!tstate->foundTraceFn) {
      if (curEvent->ins.fnId == tstate->traceId) {
	tstate->foundTraceFn = 1;
	tstate->endStackTid = curEvent->ins.tid;
	tstate->endStackPtr = *((uint64_t *) getRegisterVal(readerState,
							    LYNX_RSP,
							    tstate->endStackTid));
	tstate->run = (tstate->foundBeginFn && tstate->foundTraceFn);
      }
    }

    if (!tstate->run) {
      return 0;
    }
  }

  return 1;
}

/********************************************************************************
 *                                                                              *
 * print_operands_after() -- prints the values of an instruction's operands     *
 * after that instruction's execution.                                          *
 *                                                                              *
 ********************************************************************************/

/*
 * For technical reasons, it is convenient to record  src and dest operand values
 * for each instruction before that instruction is executed; this is what the
 * tracer does.  As a result, when the reader reads an instruction in an execution
 * trace, the register and memory values correspond to the program state *before* 
 * that instruction executes.  But people find it easier to understand a program's
 * behavior by associating each instruction with register and memory values *after*
 * that instruction has executed; this is what trace2ascii prints out.  To make 
 * this work, trace2ascii proceeds as follows: after reading an event (which gives
 * the program state before the current instruction, i.e., after the previous 
 * instruction) it first computes and prints out the operand values for the previous
 * instruction (i.e., after the previous instruction executed), then prints a 
 * newline, then prints out non-operand-value information about the current 
 * instruction.  The variable info holds information about the operands of an 
 * instruction, and this variable is not updated until after the operand values for
 * the previous instruction are printed out.
 */
void print_operand_info_after(ReaderState *readerState,
			      InsInfo *info,
			      ReaderEvent *curEvent) {
  ReaderOp *curOp = info->readWriteOps;
  int i;

  for(i = 0; i < info->readWriteOpCnt; i++) {
    if(curOp->type == MEM_OP) {
      printMemOp(readerState, "MW", curOp, curEvent->ins.tid);
    }
    else if (curOp->type == REG_OP) {
      printRegOp(readerState, "W", curOp->reg, curEvent->ins.tid);
    }
    curOp = curOp->next;
  }

  curOp = info->dstOps;
  for (i = 0; i < info->dstOpCnt; i++) {
    if (curOp->type == MEM_OP) {
      printMemOp(readerState, "MW", curOp, curEvent->ins.tid);
    }
    else if (curOp->type == REG_OP) {
      printRegOp(readerState, "W", curOp->reg, curEvent->ins.tid);
    }
    curOp = curOp->next;
  }
}

/*******************************************************************************
 *                                                                             *
 * init_trace2ascii_state() -- initialize program state.                       *
 *                                                                             *
 *******************************************************************************/

ReaderState *init_trace2ascii_state(int argc, char *argv[], Trace2Ascii_state *tstate) {
  if (argc < 2) {
    printUsage(argv[0]);
    exit(1);
  }

  parseCommandLine(argc, argv, tstate);
  ReaderState *readerState = initReader(tstate->trace, 0);

  tstate->hasSrcReg = hasFields(readerState, getSelMask(SEL_SRCREG));
  tstate->hasMemRead = hasFields(readerState, getSelMask(SEL_MEMREAD));
  tstate->hasData = (hasFields(readerState, getSelMask(SEL_DESTREG))
		     || hasFields(readerState, getSelMask(SEL_MEMWRITE))
		     || tstate->hasSrcReg
	 	     || tstate->hasMemRead);
  tstate->hasSrcId = hasFields(readerState, getSelMask(SEL_SRCID));
  tstate->hasFnId = hasFields(readerState, getSelMask(SEL_FNID));
  tstate->hasAddr = hasFields(readerState, getSelMask(SEL_ADDR));
  tstate->hasBin = hasFields(readerState, getSelMask(SEL_BIN));
  tstate->hasTid = hasFields(readerState, getSelMask(SEL_TID));

  if (tstate->beginFn) {
    tstate->beginId = findString(readerState, tstate->beginFn);
    if (tstate->beginId == -1) {
      tstate->beginFn = NULL;
    }
  }

  if (tstate->endFn) {
    tstate->endId = findString(readerState, tstate->endFn);
    if(tstate->endId == -1) {
      tstate->endFn = NULL;
    }
  }

  if (tstate->traceFn) {
    tstate->traceId = findString(readerState, tstate->traceFn);
    if (tstate->traceId == -1) {
      tstate->traceFn = NULL;
    }
  }

  tstate->foundBeginFn = (tstate->beginFn == NULL);
  tstate->foundTraceFn = (tstate->traceFn == NULL);
  tstate->run = tstate->foundBeginFn && tstate->foundTraceFn;
  tstate->addrSize = getAddrSize(readerState);
  tstate->endStackPtr = 0;
  tstate->endStackTid = -1;

  return readerState;
}

/*******************************************************************************
 *                                                                             *
 * get_next_event() -- get the next event, update prev_addr                    *
 *                                                                             *
 *******************************************************************************/

uint32_t get_next_event(ReaderState *readerState,
			ReaderEvent *curEvent,
			uint64_t *prev_addr) {
  if (curEvent != NULL && curEvent->type == INS_EVENT) {
    *prev_addr = curEvent->ins.addr;
  }

  return nextEvent(readerState, curEvent);
}

/*******************************************************************************
 *                                                                             *
 * main()                                                                      *
 *                                                                             *
 *******************************************************************************/

int main(int argc, char *argv[]) {
  Trace2Ascii_state tstate;
  uint64_t prev_addr = 0, curr_addr = 0;
  char first = 1, print_instr = 1, ins_printed = 0;
  ReaderEvent curEvent;
  InsInfo info;

  ReaderState *readerState = init_trace2ascii_state(argc, argv, &tstate);

  initInsInfo(&info);

  uint64_t n = 0;
  while (get_next_event(readerState, &curEvent, &prev_addr)) {
    if (curEvent.type == INS_EVENT) {
      int do_trace = chk_trace_state(&curEvent, readerState, &tstate);
      if (do_trace == 0) {
	continue;
      }
    
      if (first) {
	first = 0;
      }
      else {
	if (tstate.target_addr == 0 || tstate.target_addr == prev_addr) {
	  if (tstate.hasBin && tstate.hasData) {
	    print_operand_info_after(readerState, &info, &curEvent);
	  }
	}
      }

      if (ins_printed) {
	printf(";\n");
	ins_printed = 0;
      }

      if (tstate.endStackTid == curEvent.ins.tid) {
	uint64_t stackPtr = *((uint64_t *) getRegisterVal(readerState,
							  LYNX_RSP,
							  tstate.endStackTid));
	if(stackPtr > tstate.endStackPtr) {
	  break;
	}
      }
      if (curEvent.ins.fnId == tstate.endId) {
	break;
      }

      uint64_t this_instr = n++;

      if (tstate.hasBin) {
	/*
         * Update information about the source and destination operands of
         * the instruction.
         */
	fetchInsInfo(readerState, &curEvent.ins, &info);
	curr_addr = curEvent.ins.addr;
      }

      print_instr = (tstate.target_addr == 0
		     || (tstate.hasAddr && curEvent.ins.addr == tstate.target_addr));

      if (!print_instr) {
	continue;
      }
      
      ins_printed = 1;
      printf("%ld:", this_instr);

      if (tstate.hasTid) {
	printf(" %d;", curEvent.ins.tid);
      }

      if (tstate.hasAddr) {
	printf(" 0x%llx;", (unsigned long long) curEvent.ins.addr);
      }

      if (tstate.hasSrcId) {
	printf(" %s;", fetchStrFromId(readerState, curEvent.ins.srcId));
      }

      if (tstate.hasFnId) {
	printf(" %s;", fetchStrFromId(readerState, curEvent.ins.fnId));
      }

      if (tstate.hasBin) {
	int i;
	for (i = 0; i < curEvent.ins.binSize; i++) {
	  printf(" %02x", curEvent.ins.binary[i]);
	}
	
	printf("; %s; ", info.mnemonic);

	if (tstate.hasData) {
	  ReaderOp *curOp = info.srcOps;
	  for (i = 0; i < info.srcOpCnt; i++) {
	    if (curOp->type == MEM_OP) {
	      printMemOp(readerState, "MR", curOp, curEvent.ins.tid);
	    }
	    else if (curOp->type == REG_OP) {
	      printRegOp(readerState, "R", curOp->reg, curEvent.ins.tid);
	    }
	    curOp = curOp->next;
	  }

	  curOp = info.readWriteOps;
	  for (i = 0; i < info.readWriteOpCnt; i++) {
	    if (curOp->type == MEM_OP) {
	      printMemOp(readerState, "MR", curOp, curEvent.ins.tid);
	    }
	    else if (curOp->type == REG_OP) {
	      printRegOp(readerState, "R", curOp->reg, curEvent.ins.tid);
	    }
	    curOp = curOp->next;
	  }
	}
      }
    }
    else if (curEvent.type == EXCEPTION_EVENT) {
      fflush(stdout);
      printf("EXCEPTION %d at %llx\n",
	     curEvent.exception.code,
	     (unsigned long long) curEvent.exception.addr);
      fflush(stdout);
      fprintf(stderr,
	      "EXCEPTION %d at %llx\n",
	      curEvent.exception.code,
	      (unsigned long long) curEvent.exception.addr);
    }
    else {
      printf("UNKNOWN EVENT TYPE\n");
    }
  }    /* while */

  freeInsInfo(&info);
  closeReader(readerState);;

  return 0;
}
