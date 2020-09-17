

#include "Taint.h"
#include <stdio.h>
#include <stdlib.h>
#include "LabelStore.h"
#include "ByteMemoryTaint.h"
#include "ByteRegTaint.h"
#include "BytePropagation.h"
#include "TaintState.h"

/*
 * initTaint() -- initializes a taint propagation state.  This function should be
 * called at the beginning of any taint analysis.  The code below initializes the
 * taint propagation functions for byte-level propagation; if a different policy is
 * needed (e.g., bit-level or word-level), these function pointers should be set 
 * appropriately.
 */
TaintState *initTaint(ReaderState *readerState) {
  TaintState *state = malloc(sizeof(TaintState));

  state->memTaint = initByteMemTaint();
  state->freeMemTaint = (void (*) (MemoryTaint)) freeByteMemTaint;
  state->resetMemLabels = (void (*) (MemoryTaint)) resetByteMemLabels;

  state->setMemTaint =
    (void (*) (MemoryTaint, uint64_t, uint32_t, const uint64_t *))
    setByteMemTaint;

  state->setAllMemTaint =
    (void (*) (MemoryTaint, uint64_t, uint32_t, uint64_t)) setAllByteMemTaint;

  state->getMemTaint =
    (void (*) (MemoryTaint, uint64_t, uint32_t, uint64_t *)) getByteMemTaint;

  state->addTaintToMem =
    (void (*) (MemoryTaint, LabelStoreState *, uint64_t, uint32_t, uint64_t))
    addTaintToByteMem;

  state->regTaint = initByteRegTaint(getNumThreads(readerState));

  state->freeRegTaint = (void (*) (RegisterTaint)) freeByteRegTaint;

  state->setAllRegTaint =
    (void (*) (RegisterTaint, LynxReg, uint32_t, uint64_t))
    setAllByteRegTaint;

  state->setRegTaint =
    (void (*) (RegisterTaint, LynxReg, uint32_t, const uint64_t *))
    setByteRegTaint;

  state->getRegTaint =
    (const uint64_t *(*) (RegisterTaint, LynxReg, uint32_t))
    getByteRegTaint;

  state->addTaintToReg =
    (void (*) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t))
    addTaintToByteReg;

  state->getCombinedRegTaint =
    (uint64_t (*) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t))
    getCombinedByteRegTaint;

    state->labelState = initLabelStore();

    state->readerState = readerState;

    initByteHandlers(state->opHandlers);

    return state;
}


/*
 * freeMemTaintLabels() -- resets the state of the shadow memory to be clean.
 */
void freeMemTaintLabels(TaintState *state){
    state->resetMemLabels(state->memTaint);
}


/*
 * freeTaint() -- frees all memory associated with the current taint state.
 */
void freeTaint(TaintState *state) {
    state->freeMemTaint(state->memTaint);
    state->freeRegTaint(state->regTaint);
    freeLabelStore(state->labelState);
    free(state);
}


/*
 * getNewLabel() -- create and return a new taint label.
 */
uint64_t getNewLabel(TaintState *state) {
  return createNewLabel(state->labelState);
}


/*
 * addTaintToAddrCalc() -- given an operand op and a taint label, if op is a memory
 * operand it adds the taint specified by label to any register that is used for 
 * its memory address calculation; in this case it returns the value 1.  If op is
 * not a memory operand there is no effect, and the value returned is 0.
 */
int addTaintToAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
  if (op->type != MEM_OP) {
    return 0;
  }

  if (op->mem.seg != LYNX_INVALID) {
    state->addTaintToReg(state->regTaint, state->labelState, op->mem.seg, tid, label);
  }

  if (!reg_is_RIP_or_RSP(op->mem.base)) {
    state->addTaintToReg(state->regTaint, state->labelState, op->mem.base, tid, label);
  }

  if (!reg_is_RIP_or_RSP(op->mem.index)) {
    state->addTaintToReg(state->regTaint, state->labelState, op->mem.index, tid, label);
  }

  return 1;
}


/*
 * addTaintToAddrCalcList() -- given a set of operands ops and a taint label, adds
 * the taint label to any register used for an address computation in ops.  The 
 * return value is 1 if any such register was encountered, 0 otherwise.
 */
int addTaintToAddrCalcList(TaintState *state,
			   ReaderOp *ops,
			   int cnt,
			   uint32_t tid,
			   uint64_t label) {
  int tainted = 0;

  int i;
  for(i = 0; i < cnt; i++) {
    tainted |= addTaintToAddrCalc(state, ops, tid, label);
    ops = ops->next;
  }

  return tainted;
}


/*
 * taintAddrCalc() -- adds taint to the address computation of a memory operand.
 * Given an operand op: if op is a memory operand, this function sets the taint
 * label for each register used for the address calculation to label, and returns
 * the value 1; if op is not a memory operand, it leaves taint unchanged and
 * returns the value 0.
 */
int taintAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
    if (op->type != MEM_OP) {
        return 0;
    }

    if (op->mem.seg != LYNX_INVALID) {
        state->setAllRegTaint(state->regTaint, op->mem.seg, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.base)) {
        state->setAllRegTaint(state->regTaint, op->mem.base, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.index)) {
        state->setAllRegTaint(state->regTaint, op->mem.index, tid, label);
    }

    return 1;
}


/*
 * taintAddrCalcList() -- given a set of operands ops, adds taint to all their
 * address calculations.  Returns 1 if some address calculation register was
 * affected, 0 otherwise.
 */
int taintAddrCalcList(TaintState *state,
		      ReaderOp *ops,
		      int cnt,
		      uint32_t tid,
		      uint64_t label) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted |= taintAddrCalc(state, ops, tid, label);
        ops = ops->next;
    }

    return tainted;
}


/*
 * getCombinedRegTaint() -- given a register reg and taint label label, returns 
 * the taint label obtained by combining label with the taint of each byte of
 * reg in the thread specified by tid.
 */
uint64_t getCombinedRegTaint(TaintState *state,
			     LynxReg reg,
			     uint32_t tid,
			     uint64_t label) {
  return state->getCombinedRegTaint(state->regTaint,
				    state->labelState,
				    reg,
				    tid,
				    label);
}


/*
 * taintOperand() -- apply the taint label label to operand op in thread-id tid:
 *
 * If op is a register reg:
 *    If reg is other than RIP or RSP, each byte of reg has its taint set to label. 
 *    If reg is RIP or RSP, nothing happens.
 *
 * If op is memory: 
 *    The memory bytes addressed by op are tainted, and any registers involved in
 *    the address computation are also tainted.
 *
 * The return value in either case is whether the taint label is nonzero.
 *    
 */
int taintOperand(TaintState *state,
		 ReaderOp *op,
		 uint32_t tid,
		 uint64_t label,
		 InsInfo *info) {
  if (op->type == REG_OP) {
    if (!reg_is_RIP_or_RSP(op->reg) && op->reg != LYNX_GFLAGS) {
      state->setAllRegTaint(state->regTaint, op->reg, tid, label);
    }
    else if (op->reg == LYNX_GFLAGS) {
      setByteFlagTaint(state->regTaint, state->labelState, tid, info->dstFlags, label);
    }
    return label != 0;
  }  /* if (op->type == REG_OP) */

  if (op->type == MEM_OP) {
    /* 
     * add taint to the memory locations addressed by the operand
     */
    state->setAllMemTaint(state->memTaint, op->mem.addr, op->mem.size, label);
    /*
     * add taint to registers used for computing 
     */
    if(op->mem.seg != LYNX_INVALID) {
      state->setAllRegTaint(state->regTaint, op->mem.seg, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.base)) {
      state->setAllRegTaint(state->regTaint, op->mem.base, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.index)) {
      state->setAllRegTaint(state->regTaint, op->mem.index, tid, label);
    }

    return label != 0;
  }  /* if (op->type == MEM_OP) */

  return 0;
}


/*
 * taintOperandList() -- apply the taint label label to the set of operands ops 
 * in thread-id tid.
 */
int taintOperandList(TaintState *state,
		     ReaderOp *ops,
		     int cnt,
		     uint32_t tid,
		     uint64_t label,
		     InsInfo *info) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted |= taintOperand(state, ops, tid, label, info);
        ops = ops->next;
    }

    return tainted;
}


/*
 * taintIns() -- propagates taint originating from a particular instruction. 
 * To do so it  taints all of the instruction writes (i.e., the read-write 
 * operands and  the destination operands) together with the instruction bytes. 
 * The instruction bytes are also tainted so as to handle self-modifying code.
 */
int taintIns(TaintState *state, ReaderEvent *event, InsInfo *info, uint64_t label) {
    int tainted = taintOperandList(state,
				   info->readWriteOps,
				   info->readWriteOpCnt,
				   event->ins.tid,
				   label,
				   info);
    taintMem(state, event->ins.addr, event->ins.binSize, label);
    tainted |= taintOperandList(state,
			       info->dstOps,
			       info->dstOpCnt,
			       event->ins.tid,
			       label,
			       info);

    return tainted;
}


/*
 * taintReg() -- set the taint of register reg in the thread specified to label.
 */
int taintReg(TaintState *state, LynxReg reg, uint32_t thread, uint64_t label) {
    state->setAllRegTaint(state->regTaint, reg, thread, label);
    return label != 0;
}


/*
 * taintMem() -- set the taint of size bytes of memory, starting at address addr,
 * to label.
 */
int taintMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label) {
    state->setAllMemTaint(state->memTaint, addr, size, label);
    return label != 0;
}


/*
 * taintMemBlock() -- set the taint of size bytes of memory, starting at address
 * addr, to be the array of taint labels given by labels.
 */
int taintMemBlock(TaintState *state, uint64_t addr, uint32_t size, uint64_t *labels) {
    state->setMemTaint(state->memTaint, addr, size, labels);
    return 1;
}


/*
 * addTaintToOperand() -- Add the taint in label to operand op:
 *
 *    -- if op is a register reg other than RIP or RSP, add taint to the reg.
 *    -- if op is a memory operand, taint the memory locations it addresses
 *              as well as any registers used in the address computation.
 */
int addTaintToOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
  if (op->type == REG_OP) {
    if (!reg_is_RIP_or_RSP(op->reg) && op->reg != LYNX_GFLAGS) {
      state->addTaintToReg(state->regTaint, state->labelState, op->reg, tid, label);
    }
    return 1;
  }

  if (op->type == MEM_OP) {
    state->addTaintToMem(state->memTaint,
			 state->labelState,
			 op->mem.addr,
			 op->mem.size,
			 label);

    if (op->mem.seg != LYNX_INVALID) {
      state->addTaintToReg(state->regTaint, state->labelState, op->mem.seg, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.base)) {
      state->addTaintToReg(state->regTaint, state->labelState, op->mem.base, tid, label);
    }

    if (!reg_is_RIP_or_RSP(op->mem.index)) {
      state->addTaintToReg(state->regTaint, state->labelState, op->mem.index, tid, label);
    }

    return 1;
  }

  return 0;
}


/*
 * addTaintToOperandList() -- Add the taint in label to a set of operands ops.
 */
int addTaintToOperandList(TaintState *state,
			  ReaderOp *ops,
			  int cnt,
			  uint32_t tid,
			  uint64_t label) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
      tainted |= addTaintToOperand(state, ops, tid, label);
        ops = ops->next;
    }

    return tainted;
}


/*
 * addTaintToMem() -- adds the taint in label to the per-byte taint in a memory
 * region of size bytes starting at address addr.
 */
int addTaintToMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label) {
    state->addTaintToMem(state->memTaint, state->labelState, addr, size, label);

    return 1;
}


/*
 * addTaintToReg() -- add the taint in label to that of register reg in thread tid.
 */
int addTaintToReg(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label) {
  state->addTaintToReg(state->regTaint, state->labelState, reg, tid, label);
  return 1;
}


/*
 * propagateForward() -- propagate taint forward through a reader event (i.e.,
 * instruction), with corresponding InsInfo structure info (see documentation
 * on the trace reader for more about ReaderEvent and InsInfo structures).
 */
uint64_t propagateForward(TaintState *state,
			  ReaderEvent *event,
			  InsInfo *info,
			  uint8_t keepRegInAddrCalc) {
  return state->opHandlers[info->insClass](state, event, info, keepRegInAddrCalc);
}


/*
 * propagateBackward() -- propagate taint backward through an instruction 
 * indicated by info and thread tid.
 */
uint64_t propagateBackward(TaintState *state, uint32_t tid, InsInfo *info) {
  return backwardBytePropagation(state, tid, info);
}


/*
 * propagateBackwardWithIns() -- propagate taint backward through instruction
 * with corresponding InsInfo info (similarly to propagateBackward), and
 * additionally taint the bytes occupied by instruction.  Tainting of the
 * instruction bytes aims to deal with self-modifying code.
 */
uint64_t propagateBackwardWithIns(TaintState *state,
				  uint32_t tid,
				  InsInfo *info,
				  ReaderIns instruction) {
  return backwardBytePropagationWithIns(state, tid, info, instruction);
}

void recoverSpace(TaintState *state, uint8_t *alive) {
    collectRegLabels(state->regTaint, alive);
    collectMemLabels(state->memTaint, alive);
    freeUnnecessary(state->labelState, alive);
}

void getCondensedLabels(TaintState *state) {
    uint8_t *labelArr = getLabelArr(state->labelState);
    collectRegLabels(state->regTaint, labelArr);
    collectMemLabels(state->memTaint, labelArr);
    condenseLabels(state->labelState, labelArr);
}

uint8_t *getLabelArray(TaintState *state) {
    return getLabelArr(state->labelState);
}

void getArchLabels(TaintState *state, uint8_t *labels) {
    collectRegLabels(state->regTaint, labels);
    collectMemLabels(state->memTaint, labels);
}

uint8_t *outputCondensedLabels(TaintState *state, uint8_t *labels) {
    return condenseLabels(state->labelState, labels);
}

/*
 * outputTaint() -- print out tainted registers and memory locations,
 * together with their taint labels, in the given taint state.
 */
void outputTaint(TaintState *state) {
    outputRegTaint(state->regTaint, state->labelState);
    outputMemTaint(state->memTaint, state->labelState);
}

uint64_t popLabel(TaintState *state, uint64_t *label) {
    return pop(state->labelState, label);
}

uint64_t subtractLabel(TaintState *state, uint64_t label, uint64_t from) {
    return subtract(state->labelState, label, from);
}

uint8_t labelIsSubsetOf(TaintState *state, uint64_t label, uint64_t in) {
    return includes(state->labelState, label, in);
}

uint64_t labelSize(TaintState *state, uint64_t label) {
    return getSize(state->labelState, label);
}

uint64_t mergeLabels(TaintState *state, uint64_t l1, uint64_t l2) {
    return combineLabels(state->labelState, l1, l2);
}

uint64_t getFirstLabel(TaintState *state, uint64_t label) {
    return getFirst(state->labelState, label);
}

uint64_t getLastLabel(TaintState *state, uint64_t label) {
    return getLast(state->labelState, label);
}

uint64_t getLabelIntersect(TaintState *state, uint64_t l1, uint64_t l2) {
    return intersectLabels(state->labelState, l1, l2);
}

void printBaseLabels(TaintState *state, uint64_t label) {
    outputBaseLabels(state->labelState, label);
}

uint64_t *getSubLabels(TaintState *state, uint64_t label){
    return(returnSubLabels(state->labelState, label));
}

uint8_t hasLabelInRange(TaintState *state, uint64_t label, uint64_t min, uint64_t max) {
    return hasInRange(state->labelState, label, min, max);
}

uint8_t labelHasSequential(TaintState *state, uint64_t world, uint64_t label) {
    return hasSequential(state->labelState, world, label);
}

uint64_t getMemTaintLabel(TaintState *state, uint64_t addr, uint32_t sizeInBytes, uint64_t init) {
    return getCombinedByteMemTaint(state->memTaint, state->labelState, addr, sizeInBytes, init);
}

uint64_t getLabelArraySize(TaintState *state) {
    return getLabelArrSize(state->labelState);
}
