/*
* File: ShadowRegisters.h
* Author: David Raicher
* Description: Provides the shadow register backend for the trace reader.  Allows for a completely arbitrary amount
* of threads to be represented.  Each thread has a full set of registers as defined in LynxReg.  So the amount and size
* of registers should dynamically update with LynxReg.  The registers can be read or written at will.
*/

#ifndef _SHADOWREGISTERS_H_
#define _SHADOWREGISTERS_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <LynxReg.h>

typedef struct RegState_t RegState;

RegState *initShadowRegisters(uint32_t numThreadsInit);

void freeShadowRegisters(RegState *state);

void setThreadVal(RegState *state, LynxReg reg, uint32_t thread, uint8_t * val);

const uint8_t *getThreadVal(RegState *state, LynxReg reg, uint32_t thread);

#endif /* _SHADOWREGISTERS_H_ */
