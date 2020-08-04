/*
* File: ShadowMemory.h
* Author: Jon Stephens and David Raicher
* Description: This provides a Shadow Memory backend for the trace reader.  It does this implementing a quad map structure to provide byte level
* addressing for memory reads and writes.  The main interfaces for interacting with the Shadow Memory are the store and load functions.
*/

#ifndef SHADOWMEMORY_H_
#define SHADOWMEMORY_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct MemState_t MemState;

MemState *initShadowMemory();

void freeShadowMemory(MemState *state);

void store(MemState *state, uint64_t addr, uint32_t sizeInBytes, uint8_t * val);

void load(MemState *state, uint64_t addr, uint32_t sizeInBytes, uint8_t * buf);

void chkDefaultVals();

void isDefault(MemState *state);

void reset(MemState *state);

const uint8_t *getNextMemRegion(MemState *state, uint64_t *addr, uint32_t *size);

#endif /* SHADOWMEMORY_H_ */
