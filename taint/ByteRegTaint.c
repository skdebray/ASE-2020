/*
* File: ShadowRegisters.h
* Author: David Raicher
* Description: Provides the shadow register backend for the trace reader.  
* Allows for arbitrarily many threads to be represented.  Each thread 
* has a full set of registers as defined in LynxReg.  So the amount and 
* size of registers should dynamically update with LynxReg.  The registers 
* can be read or written at will.
*/

#include "ByteRegTaint.h"
#include "ReaderUtils.h"

#define NUM_OF_REGS (LYNX_LAST_FULL - LYNX_FIRST + 1)

struct ByteRegState_t {
    uint64_t ***threads ;
    uint32_t numThreads ;
};

void outputRegTaint(ByteRegState *state, LabelStoreState *labelState) {
    int i, j, k;
    for(i = 0; i < state->numThreads; i++) {
        for(j = LYNX_FIRST; j < LYNX_LAST_FULL; j++) {
            int size = LynxRegSize(j);
            if(j == LYNX_GFLAGS) {
                size *= 8;
            }

            for(k = 0; k < size; k++) {
                if(state->threads[i][j][k]) {
                    printf("%s[%d] ", LynxReg2Str(j), i);
                    outputBaseLabels(labelState, state->threads[i][j][k]);
                    break;
                }
            }
        }
    }
}

void collectRegLabels(ByteRegState *state, uint8_t *labels) {
    int i, j, k;
    for(i = 0; i < state->numThreads; i++) {
        for(j = LYNX_FIRST; j < LYNX_LAST_FULL; j++) {
            int size = LynxRegSize(j);
            if(j == LYNX_GFLAGS) {
                size *= 8;
            }

            for(k = 0; k < size; k++) {
                labels[state->threads[i][j][k]] = 1;
            }
        }
    }
}

/*
 * initByteRegTaint(uint32_t numThreadsInit) -- Initalizes the shadow registers 
 * with the designated amount of threads.  This function must be called before
 * any other functions are called.
*/
ByteRegState *initByteRegTaint(uint32_t numThreadsInit) {
  ByteRegState *state = malloc(sizeof(ByteRegState));
  if (state == NULL) {
    throwError("Out of memory: ByteRegState");
  }
  state->numThreads = numThreadsInit;
  state->threads = malloc(sizeof (uint8_t ***) * numThreadsInit);
  uint64_t ***threads = state->threads;

  if (state->threads == NULL) {
    throwError("Out of memory: shadow register threads");
  }

  uint32_t i;
  LynxReg j;
  for (i = 0; i < numThreadsInit; i++) {
    threads[i] = malloc(sizeof (uint8_t **) * NUM_OF_REGS);
    if (threads[i] == NULL) {
      throwError("Out of memory: thread in shadow registers");
    }
    
    for (j = LYNX_FIRST; j <= LYNX_LAST_FULL; j++) {
      int size = LynxRegSize(j);
      if(j == LYNX_GFLAGS) {
	size *= 8;
      }

      threads[i][j] = malloc(sizeof(uint64_t) * size);
      if (threads[i][j] == NULL) {
	throwError("Out of memory: shadow register thread");
      }

      uint32_t k;
      for (k = 0; k < size; k++) {
	threads[i][j][k] = 0;
      }
    }
  }
  
  return state;
}

/*
 * freeShadowRegisters() -- Frees all memory allocated by the shadow registers.
*/
void freeByteRegTaint(ByteRegState *state) {
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
 * getRegisterVal(LynxReg reg, uint32_t thread) -- Returns a pointer to the 
 * beginning of the shadow register that represents reg in the thread number
 * represented by thread.  If thread is larger than the amount of threads 
 * initialized in ShadowMemory, NULL is returned.
 */
const uint64_t *getByteRegTaint(ByteRegState *state,
				LynxReg reg,
				uint32_t thread) {
    if (thread >= state->numThreads) {
	    return NULL;
    }
    return state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);
}


/*
 * getCombinedByteRegTaint() -- given a register reg and taint label initLabel,
 * returns the taint label obtained by combining initLabel with the taint of
 * each byte of register reg (in the thread specified).
 */
uint64_t getCombinedByteRegTaint(ByteRegState *state,
				 LabelStoreState *labelState,
				 LynxReg reg,
				 uint32_t thread,
				 uint64_t initLabel) {
    if (thread >= state->numThreads) {
        return initLabel;
    }

    uint64_t *currentReg
      = state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);

    uint8_t sizeInBytes = LynxRegSize(reg);
    if (reg == LYNX_GFLAGS) {
        sizeInBytes *= 8;
    }

    uint64_t lastLabel = 0;

    int i;
    for (i = 0; i < sizeInBytes; i++) {
        if (initLabel != currentReg[i] && lastLabel != currentReg[i]) {
	  initLabel = combineLabels(labelState, currentReg[i], initLabel);
	  lastLabel = currentReg[i];
        }
    }

    return initLabel;
}

uint64_t getByteFlagTaint(ByteRegState *state,
			  LabelStoreState *labelState,
			  uint32_t thread,
			  uint32_t flags,
			  uint64_t curLabel) {
    uint32_t curFlag = 1;
    uint64_t *flagTaint = state->threads[thread][LYNX_GFLAGS];

    int i;
    for(i = 0; i < 32; i++) {
        if((flags & curFlag) && (flagTaint[i] != curLabel)) {
            curLabel = combineLabels(labelState, flagTaint[i], curLabel);
        }
        curFlag <<= 1;
    }

    return curLabel;
}

void setByteFlagTaint(ByteRegState *state,
		      LabelStoreState *labelState,
		      uint32_t thread,
		      uint32_t flags,
		      uint64_t curLabel) {
    uint32_t curFlag = 1;
    uint64_t *flagTaint = state->threads[thread][LYNX_GFLAGS];
   
    int i;  
    for(i = 0; i < 32; i++) {
        if(flags & curFlag) {
            flagTaint[i] = curLabel;
        }   
        curFlag <<= 1;
    }   
}

void combineByteFlagTaint(ByteRegState *state,
			  LabelStoreState *labelState,
			  uint32_t thread,
			  uint32_t flags,
			  uint64_t apply) {
    uint32_t curFlag = 1;
    uint64_t *flagTaint = state->threads[thread][LYNX_GFLAGS];

    int i;
    for(i = 0; i < 32; i++) {
        if(flags & curFlag && flagTaint[i] != apply) {
            flagTaint[i] = combineLabels(labelState, flagTaint[i], apply);
        }   
        curFlag <<= 1;
    }   
}

void addTaintToByteReg(ByteRegState *state,
		       LabelStoreState *labelState,
		       LynxReg reg,
		       uint32_t tid,
		       uint64_t label) {
    uint64_t *currentReg
      = state->threads[tid][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);

    uint8_t sizeInBytes = LynxRegSize(reg);
    
    if(reg == LYNX_GFLAGS) {
        sizeInBytes *= 8;
    }

    applyLabel(labelState, label, currentReg, sizeInBytes);
}


/*
 * setAllByteRegTaint() -- sets the taint for each byte of register reg, for
 * the thread specified, to label.
 */
void setAllByteRegTaint(ByteRegState *state,
			LynxReg reg,
			uint32_t thread,
			uint64_t label) {
    uint64_t *currentReg =
      state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);

    uint8_t sizeInBytes = LynxRegSize(reg);

    if(reg == LYNX_GFLAGS) {
        sizeInBytes *= 8;
    }

    int i;
    for(i = 0; i < sizeInBytes; i++) {
        currentReg[i] = label;
    }
}

/*
 * setByteRegTaint() -- Sets the taint for byte i of register reg, for
 * the thread specified, to labels[i].  
 */
void setByteRegTaint(ByteRegState *state,
		     LynxReg reg,
		     uint32_t thread,
		     const uint64_t *labels) {
  uint64_t *cur_reg;

  cur_reg
    = state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);
  uint8_t sizeInBytes = LynxRegSize(reg);  
    
  if (reg == LYNX_GFLAGS) {
    sizeInBytes *= 8;
  }

  int i;
  for(i = 0; i < sizeInBytes; i++) {
    cur_reg[i] = labels[i];
  }
}

void setByteRegTaintLoc(ByteRegState *state,
			LynxReg reg,
			uint32_t thread,
			uint32_t loc,
			uint64_t label) {
  uint64_t *cur_reg;
  cur_reg = state->threads[thread][LynxReg2FullLynxReg(reg)] + LynxRegOffset(reg);
  cur_reg[loc] = label;
}
