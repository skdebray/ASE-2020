/*
* File: ShadowRegisters.h
* Author: David Raicher
* Description: Provides the shadow register backend for the trace reader.  Allows for a completely arbitrary amount
* of threads to be represented.  Each thread has a full set of registers as defined in LynxReg.  So the amount and size
* of registers should dynamically update with LynxReg.  The registers can be read or written at will.
*/

#ifndef _BYTEREGTAINT_H_
#define _BYTEREGTAINT_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <LynxReg.h>
#include "LabelStore.h"

typedef struct ByteRegState_t ByteRegState;

ByteRegState *initByteRegTaint(uint32_t numThreadsInit);

void freeByteRegTaint(ByteRegState *state);

void setAllByteRegTaint(ByteRegState *state, LynxReg reg, uint32_t thread, uint64_t label);

void setByteRegTaint(ByteRegState *state, LynxReg reg, uint32_t thread, const uint64_t *labels);

void setByteRegTaintLoc(ByteRegState *state, LynxReg reg, uint32_t thread, uint32_t ind, uint64_t label);

const uint64_t *getByteRegTaint(ByteRegState *state, LynxReg reg, uint32_t thread);

uint64_t getCombinedByteRegTaint(ByteRegState *state, LabelStoreState *labelState, LynxReg reg, uint32_t thread, uint64_t initLabel);

uint64_t getByteFlagTaint(ByteRegState *state, LabelStoreState *labelState, uint32_t thread, uint32_t flags, uint64_t curLabel);

void setByteFlagTaint(ByteRegState *state, LabelStoreState *labelState, uint32_t tid, uint32_t flags, uint64_t curLabel);

void combineByteFlagTaint(ByteRegState *state, LabelStoreState *labelState, uint32_t tid, uint32_t flags, uint64_t apply);

void collectRegLabels(ByteRegState *state, uint8_t *labels);

void outputRegTaint(ByteRegState *state, LabelStoreState *labelState);

void addTaintToByteReg(ByteRegState *state, LabelStoreState *labelStore, LynxReg reg, uint32_t tid, uint64_t label);

#endif /* _SHADOWREGISTERS_H_ */
