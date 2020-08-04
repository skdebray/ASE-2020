/*
* File: ShadowMemory.h
* Author: Jon Stephens and David Raicher
* Description: This provides a Shadow Memory backend for the trace reader.  It does this implementing a quad map structure to provide byte level
* addressing for memory reads and writes.  The main interfaces for interacting with the Shadow Memory are the store and load functions.
*/

#ifndef BYTE_MEM_TAINT_H_
#define BYTE_MEM_TAINT_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "LabelStore.h"

typedef struct ByteMemTaint_t ByteMemTaint;

ByteMemTaint *initByteMemTaint();

void resetByteMemLabels(ByteMemTaint *state);

void freeByteMemTaint(ByteMemTaint *state);

void setAllByteMemTaint(ByteMemTaint *state, uint64_t addr, uint32_t sizeInBytes, uint64_t label);

void setByteMemTaint(ByteMemTaint *state, uint64_t addr, uint32_t sizeInBytes, const uint64_t * labels);

void getByteMemTaint(ByteMemTaint *state, uint64_t addr, uint32_t sizeInBytes, uint64_t * buf);

uint64_t getCombinedByteMemTaint(ByteMemTaint *state, LabelStoreState *labelState, uint64_t addr, uint32_t sizeInBytes, uint64_t init);

void chkByteMemDefaultVals();

void isByteMemTainted(ByteMemTaint *state);

void resetByteMemTaint(ByteMemTaint *state);

void collectMemLabels(ByteMemTaint *state, uint8_t *labels);

void outputMemTaint(ByteMemTaint *state, LabelStoreState *labelState);

void addTaintToByteMem(ByteMemTaint *state, LabelStoreState *labelState, uint64_t addr, uint32_t size, uint64_t label);

#endif /* SHADOWMEMORY_H_ */
