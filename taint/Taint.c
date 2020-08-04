

#include "Taint.h"
#include <stdio.h>
#include <stdlib.h>
#include "LabelStore.h"
#include "ByteMemoryTaint.h"
#include "ByteRegTaint.h"
#include "BytePropagation.h"
#include "TaintState.h"

TaintState *initTaint(ReaderState *readerState) {
    TaintState *state = malloc(sizeof(TaintState));

    state->memTaint = initByteMemTaint();
    state->freeMemTaint = (void (*) (MemoryTaint)) freeByteMemTaint;
    state->resetMemLabels = (void (*) (MemoryTaint)) resetByteMemLabels;
    state->setMemTaint = (void (*) (MemoryTaint, uint64_t, uint32_t, const uint64_t *)) setByteMemTaint;
    state->setAllMemTaint = (void (*) (MemoryTaint, uint64_t, uint32_t, uint64_t)) setAllByteMemTaint;
    state->getMemTaint = (void (*) (MemoryTaint, uint64_t, uint32_t, uint64_t *)) getByteMemTaint;
    state->addTaintToMem = (void (*) (MemoryTaint, LabelStoreState *, uint64_t, uint32_t, uint64_t)) addTaintToByteMem;

    state->regTaint = initByteRegTaint(getNumThreads(readerState));
    state->freeRegTaint = (void (*) (RegisterTaint)) freeByteRegTaint;
    state->setAllRegTaint = (void (*) (RegisterTaint, LynxReg, uint32_t, uint64_t)) setAllByteRegTaint;
    state->setRegTaint = (void (*) (RegisterTaint, LynxReg, uint32_t, const uint64_t *)) setByteRegTaint;
    state->getRegTaint = (const uint64_t *(*) (RegisterTaint, LynxReg, uint32_t)) getByteRegTaint;
    state->addTaintToReg = (void (*) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t)) addTaintToByteReg;
    state->getCombinedRegTaint = (uint64_t (*) (RegisterTaint, LabelStoreState *, LynxReg, uint32_t, uint64_t)) getCombinedByteRegTaint;

    state->labelState = initLabelStore();

    state->readerState = readerState;

    initByteHandlers(state->opHandlers);

    return state;
}

void freeMemTaintLabels(TaintState *state){
    state->resetMemLabels(state->memTaint);
}

void freeTaint(TaintState *state) {
    state->freeMemTaint(state->memTaint);
    state->freeRegTaint(state->regTaint);
    freeLabelStore(state->labelState);
    free(state);
}

uint64_t getNewLabel(TaintState *state) {
    return createNewLabel(state->labelState);
}

int addTaintToAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
    if(op->type != MEM_OP) {
        return 0;
    }

    if(op->mem.seg != LYNX_INVALID) {
        state->addTaintToReg(state->regTaint, state->labelState, op->mem.seg, tid, label);
    }

    if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP) {
        state->addTaintToReg(state->regTaint, state->labelState, op->mem.base, tid, label);
    }

    if(op->mem.index != LYNX_INVALID && op->mem.index != LYNX_RIP && op->mem.index != LYNX_RSP) {
        state->addTaintToReg(state->regTaint, state->labelState, op->mem.index, tid, label);
    }

    return 1;
}

int addTaintToAddrCalcList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted = addTaintToAddrCalc(state, ops, tid, label) || tainted;
        ops = ops->next;
    }

    return tainted;
}

int taintAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
    if(op->type != MEM_OP) {
        return 0;
    }

    if(op->mem.seg != LYNX_INVALID) {
        state->setAllRegTaint(state->regTaint, op->mem.seg, tid, label);
    }

    if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP) {
        state->setAllRegTaint(state->regTaint, op->mem.base, tid, label);
    }

    if(op->mem.index != LYNX_INVALID && op->mem.index != LYNX_RIP && op->mem.index != LYNX_RSP) {
        state->setAllRegTaint(state->regTaint, op->mem.index, tid, label);
    }

    return 1;
}

int taintAddrCalcList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted = taintAddrCalc(state, ops, tid, label) || tainted;
        ops = ops->next;
    }

    return tainted;
}

uint64_t getCombinedRegTaint(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label) {
    return state->getCombinedRegTaint(state->regTaint, state->labelState, reg, tid, label);
}

int taintOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label, InsInfo *info) {
    if(op->type == REG_OP) {
        if(op->reg != LYNX_RIP && op->reg != LYNX_RSP && op->reg != LYNX_GFLAGS) {
            state->setAllRegTaint(state->regTaint, op->reg, tid, label);
        } else if (op->reg == LYNX_GFLAGS){
          setByteFlagTaint(state->regTaint, state->labelState, tid, info->dstFlags, label);
        }
        return label != 0;
    }

    if(op->type == MEM_OP) {
        state->setAllMemTaint(state->memTaint, op->mem.addr, op->mem.size, label);

        if(op->mem.seg != LYNX_INVALID) {
            state->setAllRegTaint(state->regTaint, op->mem.seg, tid, label);
        }

        if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP) {
            state->setAllRegTaint(state->regTaint, op->mem.base, tid, label);
        }

        if(op->mem.index != LYNX_INVALID && op->mem.index != LYNX_RIP && op->mem.index != LYNX_RSP) {
            state->setAllRegTaint(state->regTaint, op->mem.index, tid, label);
        }

        return label != 0;
    }

    return 0;
}

int taintOperandList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label, InsInfo *info) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted = taintOperand(state, ops, tid, label, info) || tainted;
        ops = ops->next;
    }

    return tainted;
}

int taintIns(TaintState *state, ReaderEvent *event, InsInfo *info, uint64_t label) {
    int tainted = taintOperandList(state, info->readWriteOps, info->readWriteOpCnt, event->ins.tid, label, info);
    taintMem(state, event->ins.addr, event->ins.binSize, label);
    tainted = taintOperandList(state, info->dstOps, info->dstOpCnt, event->ins.tid, label, info) || tainted;

    return tainted;
}

int taintReg(TaintState *state, LynxReg reg, uint32_t thread, uint64_t label) {
    state->setAllRegTaint(state->regTaint, reg, thread, label);
    return label != 0;
}

int taintMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label) {
    state->setAllMemTaint(state->memTaint, addr, size, label);
    return label != 0;
}

int taintMemBlock(TaintState *state, uint64_t addr, uint32_t size, uint64_t *labels) {
    state->setMemTaint(state->memTaint, addr, size, labels);
    return 1;
}


int addTaintToOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label) {
    if(op->type == REG_OP) {
        if(op->reg != LYNX_RIP && op->reg != LYNX_RSP && op->reg != LYNX_GFLAGS) {
            state->addTaintToReg(state->regTaint, state->labelState, op->reg, tid, label);
        }
        return 1;
    }

    if(op->type == MEM_OP) {
        state->addTaintToMem(state->memTaint, state->labelState, op->mem.addr, op->mem.size, label);

        if(op->mem.seg != LYNX_INVALID) {
            state->addTaintToReg(state->regTaint, state->labelState, op->mem.seg, tid, label);
        }

        if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP) {
            state->addTaintToReg(state->regTaint, state->labelState, op->mem.base, tid, label);
        }

        if(op->mem.index != LYNX_INVALID && op->mem.index != LYNX_RIP && op->mem.index != LYNX_RSP) {
            state->addTaintToReg(state->regTaint, state->labelState, op->mem.index, tid, label);
        }

        return 1;
    }

    return 0;
}

int addTaintToOperandList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label) {
    int tainted = 0;

    int i;
    for(i = 0; i < cnt; i++) {
        tainted = addTaintToOperand(state, ops, tid, label) || tainted;
        ops = ops->next;
    }

    return tainted;
}

int addTaintToMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label) {
    state->addTaintToMem(state->memTaint, state->labelState, addr, size, label);

    return 1;
}

int addTaintToReg(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label) {
    state->addTaintToReg(state->regTaint, state->labelState, reg, tid, label);
    return 1;
}

uint64_t propagateForward(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc) {
    return state->opHandlers[info->insClass](state, event, info, keepRegInAddrCalc);
}

uint64_t propagateBackward(TaintState *state, uint32_t tid, InsInfo *info) {
    return backwardBytePropagation(state, tid, info);
}

uint64_t propagateBackwardWithIns(TaintState *state, uint32_t tid, InsInfo *info, ReaderIns instruction) {
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
