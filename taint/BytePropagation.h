#ifndef __BYTEPROPAGATION_H_
#define __BYTEPROPAGATION_H_

#include "Taint.h"

uint64_t backwardBytePropagation(TaintState *state, uint32_t tid, InsInfo *info);
uint64_t backwardBytePropagationWithIns(TaintState *state, uint32_t tid, InsInfo *info, ReaderIns instruction);
uint64_t defaultBytePropagation(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc);
uint64_t xorBytePropagation(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc);
void initByteHandlers(uint64_t (**handlers) (TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc));

#endif
