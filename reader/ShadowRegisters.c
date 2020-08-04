/*
* File: ShadowRegisters.h
* Author: David Raicher
* Description: Provides the shadow register backend for the trace reader.  Allows for a completely arbitrary amount
* of threads to be represented.  Each thread has a full set of registers as defined in LynxReg.  So the amount and size
* of registers should dynamically update with LynxReg.  The registers can be read or written at will.
*/

#include "ShadowRegisters.h"
#include "ReaderUtils.h"

#define NUM_OF_REGS (LYNX_LAST_FULL - LYNX_FIRST + 1)

struct RegState_t {
    uint8_t ***threads ;
    uint32_t numThreads ;
};

/*
* initShadowRegisters(uint32_t numThreadsInit) -- Initalizes the shadow registers with the designated amount of threads.  This
* must be called before any other functions are called.
*/

RegState *initShadowRegisters(uint32_t numThreadsInit) {
    RegState *state = malloc(sizeof(RegState));
    if (state == NULL) {
        throwError("Couldn't allocate RegState");
    }
    state->numThreads = numThreadsInit;
    state->threads = malloc(sizeof (uint8_t ***) * numThreadsInit);
    uint8_t ***threads = state->threads;

    if (state->threads == NULL) {
        throwError("Unable to allocate space for shadow register threads");
    }

    uint32_t i;
    LynxReg j;
    for (i = 0; i < numThreadsInit; i++) {
	    threads[i] = malloc(sizeof (uint8_t **) * NUM_OF_REGS);
        if (threads[i] == NULL) {
            throwError("Unable to allocate space for thread in shadow registers");
        }
	    for (j = LYNX_FIRST; j <= LYNX_LAST_FULL; j++) {
    	    threads[i][j] = malloc(LynxRegSize (j));
            if (threads[i][j] == NULL) {
                throwError("Unable to allocate space for register in shadow register thread");
            }
    	    uint32_t k;
    	    for (k = 0; k < LynxRegSize (j); k++) {
    		    threads[i][j][k] = 0;
    	    }
    	}
    }
    return state;
}

/*
* freeShadowRegisters(void) -- Frees all memory allocated by the shadow registers.
*/

void freeShadowRegisters(RegState *state) {
    if (state->threads == NULL) {
        return;
    }

    uint32_t i;
    for (i = 0; i < state->numThreads; i++) {
	    uint32_t j;
	    for (j = 0; j < NUM_OF_REGS; j++) {
	        free(state->threads[i][j]);
	    }
	    free(state->threads[i]);
    }
    free(state->threads);
    free(state);
}

/*
* getRegisterVal(LynxReg reg, uint32_t thread) -- Returns a pointer to the beginning of the shadow register that represents reg in the
* thread number represented by thread.  If thread is larger than the amount of threads initialized in ShadowMemory, NULL is returned.
*/

const uint8_t *getThreadVal(RegState *state, LynxReg reg, uint32_t thread) {
    if (thread >= state->numThreads) {
	    return NULL;
    }
    return state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);
}

static inline void set16(uint8_t * reg, uint8_t * val) {
    *((uint16_t *) reg) = *((uint16_t *) val);
}

static inline void set32(uint8_t * reg, uint8_t * val) {
    *((uint32_t *) reg) = *((uint32_t *) val);
}

static inline void set64(uint8_t * reg, uint8_t * val) {
    *((uint64_t *) reg) = *((uint64_t *) val);
}

/*
* setThreadVal(LynxReg, uint32_t thread, uint8_t *val) -- Sets the value in the register represented by reg on the thread represented by
* thread to the sequence of bytes in val.  Assumes val is at least the length of the given register.
*/

void setThreadVal(RegState *state, LynxReg reg, uint32_t thread, uint8_t * val) {
    uint8_t *currentReg = state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);
    uint8_t sizeInBytes = LynxRegSize(reg);

    if (sizeInBytes <= 8) {
	    if (sizeInBytes == 4) {
	        set32(currentReg, val);
	        return;
	    }
	    else if (sizeInBytes == 8) {
	        set64(currentReg, val);
	        return;
	    }
	    else if (sizeInBytes == 2) {
	        set16(currentReg, val);
	        return;
	    }
	    else {
	        *currentReg = *val;
	        return;
	    }
    }

    while (sizeInBytes >= 8) {
	    set64(currentReg, val);
	    currentReg += 8;
	    val += 8;
	    sizeInBytes -= 8;
    }

    while (sizeInBytes != 0) {
	    *currentReg++ = *val++;
	    sizeInBytes--;
    }
}
