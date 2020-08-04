#ifndef __TAINT_H_
#define __TAINT_H_

#include <stdint.h>
#include <Reader.h>

typedef struct TaintState_t TaintState;

TaintState *initTaint(ReaderState *readerState);
void freeMemTaintLabels(TaintState *state);
void freeTaint(TaintState *state);
void recoverSpace(TaintState *state, uint8_t *alive);

uint64_t getNewLabel(TaintState *state);

uint64_t propagateForward(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc);
uint64_t propagateBackward(TaintState *state, uint32_t tid, InsInfo *info);
uint64_t propagateBackwardWithIns(TaintState *state, uint32_t tid, InsInfo *info, ReaderIns instruction);
int taintOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label, InsInfo *info);
int taintOperandList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label, InsInfo *info);
int taintIns(TaintState *state, ReaderEvent *event, InsInfo *info, uint64_t label);
int taintReg(TaintState *state, LynxReg reg, uint32_t thread, uint64_t label);
int taintMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label);
int taintMemBlock(TaintState *state, uint64_t addr, uint32_t size, uint64_t *labels);
int taintAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);
int taintAddrCalcList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label);

uint8_t propagateConst(TaintState *state, ReaderEvent *event, InsInfo *info);

void getCondensedLabels(TaintState *state);
uint8_t *getLabelArray(TaintState *state);
void getArchLabels(TaintState *state, uint8_t *labels);
uint8_t *outputCondensedLabels(TaintState *state, uint8_t *labels);

void outputTaint(TaintState *state);

int addTaintToReg(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label);
int addTaintToMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label);
int addTaintToOperandList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label);
int addTaintToOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);
int addTaintToAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);
int addTaintToAddrCalcList(TaintState *state, ReaderOp *ops, int cnt, uint32_t tid, uint64_t label);

uint64_t getCombinedRegTaint(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label);

uint64_t popLabel(TaintState *state, uint64_t *label);
uint64_t subtractLabel(TaintState *state, uint64_t label, uint64_t from);
uint8_t labelIsSubsetOf(TaintState *state, uint64_t label, uint64_t in);
uint64_t labelSize(TaintState *state, uint64_t label);
uint64_t mergeLabels(TaintState *state, uint64_t l1, uint64_t l2);
uint64_t getFirstLabel(TaintState *state, uint64_t label);
uint64_t getLastLabel(TaintState *state, uint64_t label);
uint64_t getLabelIntersect(TaintState *state, uint64_t l1, uint64_t l2);
uint64_t *getSubLabels(TaintState *state, uint64_t label);
void printBaseLabels(TaintState *state, uint64_t label);
uint8_t hasLabelInRange(TaintState *state, uint64_t label, uint64_t min, uint64_t max);
uint8_t labelHasSequential(TaintState *state, uint64_t world, uint64_t label);
uint64_t getMemTaintLabel(TaintState *state, uint64_t addr, uint32_t sizeInBytes, uint64_t init);
uint64_t getLabelArraySize(TaintState *state);
#endif
