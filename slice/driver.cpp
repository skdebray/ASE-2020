/*
 * File: driver.cpp
 * Purpose: The driver code for the slicer tool
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
#include "utils.h"
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

/*******************************************************************************
 *                                                                             *
 * init_slice_driver() -- initialize some values used by the driver code.      *
 *                                                                             *
 *******************************************************************************/

void init_slice_driver(char **argv, SlicedriverState *driver_state) {
  driver_state->rState = initReader(driver_state->trace, 0);
  driver_state->backTaint = initTaint(driver_state->rState);

  vector<uint32_t>::iterator it = driver_state->includeSrcs.begin();
  while (it != driver_state->includeSrcs.end()) {
    *it = findString(driver_state->rState, argv[*it]);
    
    if ((int) *it == -1) {
      it = driver_state->includeSrcs.erase(it);
    }
    else {
      it++;
    }
  }

  if (driver_state->beginFn) {
    driver_state->beginId = findString(driver_state->rState, driver_state->beginFn);
  }
  else {
    driver_state->beginId = findString(driver_state->rState, "main");
  }

  if(driver_state->endFn) {
    driver_state->endId = findString(driver_state->rState, driver_state->endFn);
  }
  else if((int)driver_state->beginId == -1) {
    driver_state->endId = findString(driver_state->rState, "_exit");
  }
  else {
    driver_state->endId = findString(driver_state->rState, "__libc_start_main");
  }

  if(driver_state->traceFn) {
    driver_state->traceId = findString(driver_state->rState, driver_state->traceFn);
  }
}


/*******************************************************************************
 *                                                                             *
 * build_cfg() -- read the trace file and build a DCFG that will be used to    *
 * compute the slice.                                                          *
 *                                                                             *
 *******************************************************************************/

static long event_posn = -1;

void build_cfg(SlicedriverState *driver_state) {
  ReaderEvent curEvent;
  cfgState *cfgs = initCFG(driver_state->rState,
			   (int)getNumThreads(driver_state->rState));

  uint8_t foundBeginFn = ((int)driver_state->beginId == -1);
  uint8_t foundTraceFn = ((int)driver_state->traceId == -1);
  uint8_t run = foundBeginFn && foundTraceFn;
  uint64_t endStackPtr = 0;
  uint32_t endStackTid = -1;
  uint64_t numIns = 0;
  
  while (nextEvent(driver_state->rState, &curEvent)) {
    event_posn++;

    if (curEvent.type == INS_EVENT) {
      if(driver_state->targetTid != -1 && curEvent.ins.tid != driver_state->targetTid) {
	continue;
      }

      if (!run) {
	if (!foundBeginFn && curEvent.ins.fnId == driver_state->beginId) {
	  foundBeginFn = 1;
	}
	if (!foundTraceFn && curEvent.ins.fnId == driver_state->traceId) {
	  foundTraceFn = 1;
	  endStackTid = curEvent.ins.tid;
	  endStackPtr = *((uint64_t *) getRegisterVal(driver_state->rState,
						      LYNX_RSP,
						      endStackTid));
	}

	run = foundBeginFn && foundTraceFn;
	if(!run) {
	  continue;
	}
      }

      if (endStackTid == curEvent.ins.tid) {
	uint64_t stackPtr = *((uint64_t *) getRegisterVal(driver_state->rState,
							  LYNX_RSP,
							  endStackTid));
	if (stackPtr > endStackPtr) {
	  break;
	}
      }
      if(curEvent.ins.fnId == driver_state->endId) {
	break;
      }
    }
    if (curEvent.type == EXCEPTION_EVENT) {
      if (!run ||
	  (driver_state->targetTid != -1
	   && curEvent.exception.tid != driver_state->targetTid)) {
	continue;
      }
      printf("EXCEPTION %d\n", curEvent.exception.code);
    }

    cfgInstruction *cfgVal = addInstructionToCFG(&curEvent, cfgs);
    if (cfgVal->event.type != EXCEPTION_EVENT) {
      /* not sure of the purpose of this empty IF-stmt: bug???  --SKD 6/2020 */
    }
    
    numIns++;
    InsInfo *curInfo = new InsInfo();
    initInsInfo(curInfo);
    fetchInsInfo(driver_state->rState, &(cfgVal->event.ins), curInfo);
    // Save reg values
    infoTuple *saved = new infoTuple();
    saved->storedInfo = curInfo;
    if (curEvent.type == EXCEPTION_EVENT) {
      saved->tid = curEvent.exception.tid;
    }
    else {
      saved->tid = curEvent.ins.tid;
    }
    saved->event_pos = event_posn;

    // If we have a PUSH, save the rsp/reg value
    xed_iclass_enum_t inst = curInfo->insClass;
    if (!driver_state->keepReg && curEvent.type != EXCEPTION_EVENT && isPushIns(inst)) {
      ReaderOp *op = curInfo->srcOps;
      for (int i = 0; i < curInfo->srcOpCnt; i++) {
	if (op->type == REG_OP) {    // Handle Push (save REG pushed on stack)
	  LynxReg parent = LynxReg2FullLynxReg(op->reg);
	  if (parent != LYNX_RSP) {
	    saved->regVal = getRegisterVal(driver_state->rState, parent, curEvent.ins.tid);
	    saved->reg = parent;
	  }
	}
	op = op->next;
      }
      op = curInfo->dstOps;
      for (int i = 0; i < curInfo->dstOpCnt; i++) {
	if (op->type == MEM_OP) {    // Handle Push (save REG pushed on stack) 
	  saved->rspVal = op->mem.addr;
	  break;
	}
	op = op->next;
      }
    }
    else if (!driver_state->keepReg && curEvent.type != EXCEPTION_EVENT && isPopIns(inst)) {
      // handle save reg values for POP
      ReaderOp *op = curInfo->srcOps;    // Get RSP
      for (int i = 0; i < curInfo->srcOpCnt; i++) {
	if (op->type == MEM_OP) {    // Handle Pop's RSP
	  saved->rspVal = op->mem.addr;
	  break;
	}
	op = op->next;
      }
      // Get the register/reg value
      op = curInfo->dstOps;
      for (int i = 0; i < curInfo->dstOpCnt; i++) {
	if (op->type == REG_OP) {    // Handle Pop (save REG pushed on stack)
	  LynxReg parent = LynxReg2FullLynxReg(op->reg);
	  if (parent != LYNX_RSP) {
	    saved->regVal = getRegisterVal(driver_state->rState, parent, curEvent.ins.tid);
	    saved->reg = parent;
	  }
	}
	op = op->next;
      }
    }

    driver_state->insCollection->push_back(std::make_pair(cfgVal, saved));
  }
    
  driver_state->numIns = numIns;

  finalizeCFG(cfgs);

#if 0
  printNumIns(cfgs);
  printNumBlocks(cfgs);
  printNumEdges(cfgs);
#endif
}


void init_driver_state(SlicedriverState *driver_state,
		       vector<std::pair<cfgInstruction *, infoTuple *>> *insCollection) {
  driver_state->trace = NULL;
  driver_state->beginFn  = NULL;
  driver_state->beginId = -1;
  driver_state->endFn = NULL;
  driver_state->endId = -1;
  driver_state->traceFn = NULL;
  driver_state->traceId = -1;
  driver_state->targetTid = -1;
  driver_state->makeSuperblocks = false;
  driver_state->compress = false;
  driver_state->validate = false;
  driver_state->keepReg = 0;
  driver_state->print_slice = false;
  driver_state->outfile = NULL;
  driver_state->outf = stdout;
  driver_state->sliceAddr = 0;
  driver_state->slice_pos = 0;
  driver_state->insCollection = insCollection;
}

/*******************************************************************************
 *                                                                             *
 * main()                                                                      *
 *                                                                             *
 *******************************************************************************/

int main(int argc, char **argv){
  SlicedriverState driver_state;
  std::pair<Action *, uint64_t> action_info;
  vector<std::pair<cfgInstruction *, infoTuple *>> insCollection;

  init_driver_state(&driver_state, &insCollection);
    
  parseCommandLine(argc, argv, &driver_state);
    
  init_slice_driver(argv, &driver_state);

  build_cfg(&driver_state);
    
  SliceState *sliceState = compute_slice(&driver_state);

  closeReader(driver_state.rState);
    
  return 0;
}
