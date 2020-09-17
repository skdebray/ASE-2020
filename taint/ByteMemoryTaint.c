/*
 * File: ShadowMemory.c
 * Authors: Jon Stephens and David Raicher
 * Description: This provides a Shadow Memory backend for the trace reader.  
 * It does this using a quad map structure to provide byte level addressing 
 * for memory reads and writes.  The main interfaces for interacting with 
 * the Shadow Memory are the store and load functions.  If you are doing
 * development on this code please note that there are 1/2/4/8 bit versions 
 * of store/load internally to allow for faster reads/writes.
 */

#include "ByteMemoryTaint.h"
#include "ReaderUtils.h"
#include "LabelStore.h"

/**
 * Data Structure!
 * So the structure here can be a bit cryptic, especially since they are defined
 * all over the place, I'll provide a description here to hopefully make things 
 * a little easier. There are 4 levels to this structure each accessed by a 
 * portion of the address. You can think of it as a hash map with 4 levels. 
 * You access the first using the most significant quarter of the address to get 
 * to the second hash map, then so on with the third and fourth so that the fourth 
 * hash map is accessed with the least significant quarter of  the address. Each 
 * has map has a "default value" defined by a D in front of the level it is the 
 * default value for. Therefore DSM is the default value for the SM (secondary map).
 * The default values can be read from, but not written to and are initialized 
 * such that all memory values are 0. Therefore we have the  following:
 * 
 * PM: Topmost level of the hash map stored within the ByteMemTaint. Points to a SM
 * SM: Second level of the hash map. Points to a TM
 * DSM: Default SM value where all entries point to the DTM 
 * TM: Third level of the hash map, points to a QM
 * DTM: Defualt TM value where all entries point to the DQM
 * QM: The main data structure used, it stores all of the actual information
 * DQM: Default QM value where all entries are 0
 **/

#define SM QM***
#define TM QM**
#define ADDRINT uint64_t
#define LVL_WIDTH 16
#define LVL_SIZE (1 << LVL_WIDTH)
#define PM_SHAMT (LVL_WIDTH * 3)
#define SM_SHAMT (LVL_WIDTH * 2)
#define TM_SHAMT LVL_WIDTH
#define ADDR_MASK (LVL_SIZE - 1)

typedef struct QM_t {
    uint64_t label[LVL_SIZE];
} QM;

struct ByteMemTaint_t {
    QM ***PM[LVL_SIZE];        // default secondary map
};

static QM ***DSM = NULL;            // 256
static QM **DTM = NULL;
static QM *DQM = NULL;
static int refCnt = 0;

void outputMemTaint(ByteMemTaint *state, LabelStoreState *storeState) {
  int i, j, k, l;

  for (i = 0; i < LVL_SIZE; i++) {
    if (state->PM[i] != DSM) {
      for (j = 0; j < LVL_SIZE; j++) {
	if (state->PM[i][j] != DTM) {
	  for (k = 0; k < LVL_SIZE; k++) {
	    if (state->PM[i][j][k] != DQM) {
	      for (l = 0; l < LVL_SIZE; l++) {
		if (state->PM[i][j][k]->label[l]) {
		  printf("%04x%04x%04x%04x ", i, j, k, l);
		  outputBaseLabels(storeState, (unsigned long long) state->PM[i][j][k]->label[l]);
		}
	      }
	    }
	  }
	}
      }
    }
  }
}

void collectMemLabels(ByteMemTaint *state, uint8_t *labels) {
  int i, j, k, l;

  for (i = 0; i < LVL_SIZE; i++) {
    if (state->PM[i] != DSM) {
      for (j = 0; j < LVL_SIZE; j++) {
	if (state->PM[i][j] != DTM) {
	  for (k = 0; k < LVL_SIZE; k++) {
	    if (state->PM[i][j][k] != DQM) {
	      for (l = 0; l < LVL_SIZE; l++) {
		labels[state->PM[i][j][k]->label[l]] = 1;
	      }
	    }
	  }
	}
      }
    }
  }
}

/*
 * initByteMemTaint() -- Initializes the Shadow Memory.  This must be called 
 * before any other Shadow Memory functions are called.
 * freeShadowMemory should also be called before any subsequent calls to this 
 * function to prevent memory leakage.
*/
ByteMemTaint *initByteMemTaint() {
  ByteMemTaint *state = calloc(1, sizeof(ByteMemTaint));
    if (state == NULL) {
        throwError("Could not allocate space for memory state");
    }

    char init = 0;
    if (DSM == NULL) {
      DSM = calloc(1, sizeof (QM **[LVL_SIZE]));
        init = 1;
    }

    if (DTM == NULL) {
      DTM = calloc(1, sizeof (QM *[LVL_SIZE]));
        init = 1;
    }

    if (DQM == NULL) {
      DQM = calloc(1, sizeof (QM));
        init = 1;
    }

    if (DSM == NULL || DTM == NULL || DQM == NULL) {
        throwError("Unable to allocate space for default maps in shadow memory");
    }

    refCnt++;

    uint32_t i;
    for (i = 0; i < LVL_SIZE; i++) {
        state->PM[i] = DSM;
        if (init) {
            DSM[i] = DTM;
            DTM[i] = DQM;
        }
    }

    return state;
}

static inline bool is_DTM(TM curTM) {
    return DTM == curTM;
}

static inline bool is_DSM(SM curSM) {
    return DSM == curSM;
}

static inline bool is_DQM(QM * curQM) {
    return DQM == curQM;
}


/*
 * resetByteMemTaint() -- Resets the state of the shadow memory to be 
 * completely clean, i.e., just like after a call to initByteMemTaint().
*/
void resetByteMemTaint(ByteMemTaint *state) {
    uint32_t i, j, k;
    for (i = 0; i < LVL_SIZE; i++) {
        SM curSM = state->PM[i];
        if (DSM == NULL || !is_DSM(curSM)) {
            for (j = 0; j < LVL_SIZE; j++) {
                TM curTM = curSM[j];
                if (!is_DTM(curTM)) {
                    for (k = 0; k < LVL_SIZE; k++) {
                        QM *curQM = curTM[k];
                        if (!is_DQM(curQM)) {
                            free(curQM);
                            curTM[k] = DQM;
                        }
                    }

                    free(curTM);
                    curSM[j] = DTM;
                }
            }
            if (DSM != NULL) {
                free(curSM);
                state->PM[i] = DSM;
            }
        }
    }
}

static inline void freeGlobalState() {
    free(DSM);
    free(DTM);
    free(DQM);
}


/*
* freeShadowMemory() -- Frees all memory allocated by the Shadow Memory.
*/
void resetByteMemLabels(ByteMemTaint *state){
    resetByteMemTaint(state);
}

void freeByteMemTaint(ByteMemTaint *state) {
    resetByteMemTaint(state);
    free(state);
    refCnt--;
    if (refCnt == 0) {
        freeGlobalState();
    }
}

static inline SM getSM(ByteMemTaint *state, ADDRINT addr) {
    return state->PM[addr >> PM_SHAMT];
}

static inline TM getTM(ADDRINT addr, SM curSM) {
    return curSM[(addr >> SM_SHAMT) & ADDR_MASK];
}

static inline QM *getQM(ADDRINT addr, TM curTM) {
    return curTM[(addr >> TM_SHAMT) & ADDR_MASK];
}

static inline SM *getSMPtr(ByteMemTaint *state, ADDRINT addr) {
    return &state->PM[addr >> PM_SHAMT];
}

static inline TM *getTMPtr(ADDRINT addr, SM curSM) {
    return &curSM[(addr >> SM_SHAMT) & ADDR_MASK];
}

static inline QM **getQMPtr(ADDRINT addr, TM curTM) {
    return &curTM[(addr >> TM_SHAMT) & ADDR_MASK];
}


/*
 * copyForWritingTM(ADDRINT addr, TM *srcTM) -- Replaces TM in primary map, 
 * and returns the created QM.
 */
static inline QM *copyForWritingTM(ADDRINT addr, TM * srcTM) {
  TM newTM = calloc(1, sizeof (QM *[LVL_SIZE]));

    if (newTM == NULL) {
        throwError("Unable to allocate space for new TM");
    }

    memcpy(newTM, *srcTM, sizeof (QM *[LVL_SIZE]));

    QM *newQM = calloc(1, sizeof (QM));

    if (newQM == NULL) {
        throwError("Unable to allocate space for new QM");
    }

    QM **curQM = getQMPtr(addr, newTM);
    memcpy(newQM, *curQM, sizeof (QM));

    *srcTM = newTM;
    *curQM = newQM;

    return newQM;
}


/*
 * copyForWritingSM(ADDRINT addr, TM *srcTM) -- Replaces SM in primary map, 
 * and returns the created QM.
 */
static inline QM *copyForWritingSM(ADDRINT addr, SM * srcSM) {
  SM newSM = calloc(1, sizeof (QM **[LVL_SIZE]));

    if (newSM == NULL) {
        throwError("Unable to allocate space for new SM");
    }

    memcpy(newSM, *srcSM, sizeof (QM **[LVL_SIZE]));

    TM newTM = calloc(1, sizeof (QM *[LVL_SIZE]));

    if (newTM == NULL) {
        throwError("Unable to allocate space for new TM");
    }

    TM *curTM = getTMPtr(addr, newSM);
    memcpy(newTM, *curTM, sizeof (QM *[LVL_SIZE]));

    QM *newQM = calloc(1, sizeof (QM));

    if (newQM == NULL) {
        throwError("Unable to allocate space for new QM");
    }

    QM **curQM = getQMPtr(addr, newTM);
    memcpy(newQM, *curQM, sizeof (QM));

    *srcSM = newSM;
    *curTM = newTM;
    *curQM = newQM;

    return newQM;
}


/*
 * copyForWritingQM(ADDRINT addr, TM *srcTM) -- Replaces QM in primary map, 
 * and returns created QM.
 */
static inline QM *copyForWritingQM(ADDRINT addr, QM ** srcQM) {
  QM *newQM = calloc(1, sizeof (QM));

    if (newQM == NULL) {
        throwError("Unable to allocate space for new QM");
    }

    memcpy(newQM, *srcQM, sizeof (QM));

    *srcQM = newQM;

    return newQM;
}


/*
 * getQMForReading(ADDRINT addr) -- Returns the QM at the specified address 
 * for reading.
 */
static inline QM *getQMForReading(ByteMemTaint *state, ADDRINT addr) {
  uint64_t idx1 = addr >> PM_SHAMT;                /* primary */
  uint64_t idx2 = (addr >> SM_SHAMT) & ADDR_MASK;  /* secondary */
  uint64_t idx3 = (addr >> TM_SHAMT) & ADDR_MASK;  /* tertiary */

  return state->PM[idx1][idx2][idx3];
}


/*
 * getQMForWriting(ADDRINT addr) -- Returns the QM for the specified address.
 */
static inline QM *getQMForWriting(ByteMemTaint *state, ADDRINT addr) {
    SM *curSM = getSMPtr(state, addr);
    if (is_DSM(*curSM)) {
        return copyForWritingSM(addr, curSM);
    }

    TM *curTM = getTMPtr(addr, *curSM);
    if (is_DTM(*curTM)) {
        return copyForWritingTM(addr, curTM);
    }

    QM **curQM = getQMPtr(addr, *curTM);
    if (is_DQM(*curQM)) {
        return copyForWritingQM(addr, curQM);
    }

    return *curQM;
}


/*
 * isByteMemTainted() -- Checks to see which chunks of memory have been 
 * allocated (and hence are no longer the default map values).
 */
void isByteMemTainted(ByteMemTaint *state) {
    uint32_t i;
    for (i = 0; i < LVL_SIZE; i++) {
        if (DSM != NULL && !is_DSM(state->PM[i])) {
            printf("Not DSM at %d\n", i);
        }
        if (DTM != NULL && !is_DTM(DSM[i])) {
            printf("Not DTM at %d\n", i);
        }
        if (DQM != NULL && !is_DQM(DTM[i])) {
            printf("Not DQM at %d\n", i);
        }
    }
}


/*
 * chkByteMemDefaultVals() -- Checks to ensure the default map values are consistent.
 */
void chkByteMemDefaultVals(void) {
    uint32_t i;
    for (i = 0; i < LVL_SIZE; i++) {
        if (DSM != NULL && DSM[i] != DTM) {
            printf("Bad DSM at %d\n", i);
        }
        if (DTM != NULL && DTM[i] != DQM) {
            printf("Bad DTM at %d\n", i);
        }
        if (i < LVL_SIZE && DQM->label[i] != 0) {
            printf("Bad DQM at %p, val: %llx\n",
		   DQM,
		   (unsigned long long)DQM->label[i]);
        }
    }
}


/*
 * isNotAligned(ADDRINT addr, uint32_t alignment) -- Checks to see if the size 
 * of a given read/write is aligned with the QM size.  Easy check to see if a
 * write/read may be split between seperate QMs.
 */
static inline bool isNotAligned(ADDRINT addr, uint32_t alignment) {
    return addr & (alignment - 1);
}


/*
 * hasSpace(ADDRINT addr, unit32_t sizeInBytes) -- Determines if there is space 
 * on the given QM starting at addr for the given write.  Can be used to ensure 
 * there isn't a write off the side of the array. Returns 0 if there is not space,
 * 1 otherwise.
 */
static inline bool hasSpace(ADDRINT addr, uint32_t sizeInBytes) {
    return ((addr & ADDR_MASK) + sizeInBytes) < ADDR_MASK;
}


/*
 * store8(ADDRINT addr, uint8_t newVal) -- Stores newVal (a byte) at the given 
 * address.
 */
static inline void store8(ByteMemTaint *state,
			  ADDRINT addr,
			  const uint64_t *label) {
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = (addr & ADDR_MASK);
    curQM->label[i] = *label;
}


/*
 * storeSlow(ADDRINT addr, uint8_t *val, uint32_t sizeInBytes) -- Stores the 
 * bytes in the buffer val starting at addr one by one.  Obviously the slowest
 * way to store large values.  Assume val is at least sizeInBytes long.
 */
static inline void storeSlow(ByteMemTaint *state,
			     ADDRINT addr,
			     const uint64_t *labels,
			     uint32_t sizeInBytes) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        store8(state, addr + i, labels + i);
    }
}


/*
 * store16/store32/store64(ADDRINT, addr, uint8_t *val) -- Quickly stores 2/4/8 
 * bytes from val into shadow memory starting at address addr.  Assumes that
 * val is long enough for the write.
 */
static inline void store16(ByteMemTaint *state,
			   ADDRINT addr,
			   const uint64_t *labels) {
    if (isNotAligned(addr, 2)) {
        storeSlow(state, addr, labels, 2);
        return;
    }

    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    curQM->label[i++] = labels[0];
    curQM->label[i] = labels[1];
}

static inline void store32(ByteMemTaint *state,
			   ADDRINT addr,
			   const uint64_t *labels) {
    if (isNotAligned(addr, 4)) {
        storeSlow(state, addr, labels, 4);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    curQM->label[i++] = labels[0];
    curQM->label[i++] = labels[1];
    curQM->label[i++] = labels[2];
    curQM->label[i] = labels[3];

}

static inline void store64(ByteMemTaint *state,
			   ADDRINT addr,
			   const uint64_t *labels) {
    if (isNotAligned(addr, 8)) {
        storeSlow(state, addr, labels, 8);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    curQM->label[i++] = labels[0];
    curQM->label[i++] = labels[1];
    curQM->label[i++] = labels[2];
    curQM->label[i++] = labels[3];
    curQM->label[i++] = labels[4];
    curQM->label[i++] = labels[5];
    curQM->label[i++] = labels[6];
    curQM->label[i] = labels[7];
}


/*
 * setAllByteMemTaint() -- sets sizeInBytes memory locations, starting from
 * addr, to the taint label given by label.
 */
void setAllByteMemTaint(ByteMemTaint *state,
			ADDRINT addr,
			uint32_t sizeInBytes,
			uint64_t label) {
    int i;
    for(i = 0; i < sizeInBytes; i++) {
        store8(state, addr + i, &label);
    }
}


/*
 * setByteMemTaint(uint64_t addr, uint32_t sizeInBytes, uint8_t *val) --Stores 
 * the series  of bytes (up to sizeInBytes) stored in val into the shadow memory 
 * at address addr.  Assumes val is at least sizeInBytes long.
 */
void setByteMemTaint(ByteMemTaint *state,
		     ADDRINT addr,
		     uint32_t sizeInBytes,
		     const uint64_t *labels) {
    if (sizeInBytes <= 8) {
        if (sizeInBytes == 4) {
            store32(state, addr, labels);
            return;
        }
        else if (sizeInBytes == 8) {
            store64(state, addr, labels);
            return;
        }
        else if (sizeInBytes == 2) {
            store16(state, addr, labels);
            return;
        }
        else if (sizeInBytes == 1) {
            store8(state, addr, labels);
            return;
        }
    }

    while (sizeInBytes != 0 && isNotAligned(addr, 8)) {
        store8(state, addr, labels);
        addr++;
        labels++;
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        store64(state, addr, labels);
        sizeInBytes -= 8;
        labels += 8;
        addr += 8;
    }

    while (sizeInBytes > 0) {
        store8(state, addr, labels);
        addr++;
        labels++;
        sizeInBytes--;
    }
}


/*
 * load8(ADDRINT addr) -- Returns the value currently stored at addr (1 byte).
 */
static inline void load8(ByteMemTaint *state, ADDRINT addr, uint64_t *buffer) {
    QM *curQM = getQMForReading(state, addr);
    *buffer = curQM->label[addr & ADDR_MASK];
}


/*
 * loadSlow(ADDRINT addr, uint32_t sizeInBytes, uint8_t *buffer) -- Loads 
 * sizeInBytes bytes into buffer starting at addr.  This is done bytewise and
 * hence is the slowest possible way.  Assumes buffer is >= sizeInBytes long.
 */
static inline void loadSlow(ByteMemTaint *state,
			    ADDRINT addr,
			    uint32_t sizeInBytes,
			    uint64_t *buffer) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        load8(state, addr + i, buffer + i);
    }
}


/*
 * load16/load32/load64(ADDRINT, addr, uint8_t *buffer) -- Quickly loads 
 * 2/4/8 bytes from shadow memory into buffer starting at address addr.  
 * Assumes buffer is sizeInBytes long.
 */
static inline void load16(ByteMemTaint *state,
			  ADDRINT addr,
			  uint64_t * buffer) {
    if (isNotAligned(addr, 2)) {
        loadSlow(state, addr, 2, buffer);
        return;
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    buffer[0] = curQM->label[i++];
    buffer[1] = curQM->label[i];
}

static inline void load32(ByteMemTaint *state,
			  ADDRINT addr,
			  uint64_t * buffer) {
    if (isNotAligned(addr, 4)) {
        loadSlow(state, addr, 4, buffer);
        return;
    }

    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    buffer[0] = curQM->label[i++];
    buffer[1] = curQM->label[i++];
    buffer[2] = curQM->label[i++];
    buffer[3] = curQM->label[i];
}

static inline void load64(ByteMemTaint *state,
			  ADDRINT addr,
			  uint64_t * buffer) {
    if (isNotAligned(addr, 8)) {
        loadSlow(state, addr, 8, buffer);
        return;
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    buffer[0] = curQM->label[i++];
    buffer[1] = curQM->label[i++];
    buffer[2] = curQM->label[i++];
    buffer[3] = curQM->label[i++];
    buffer[4] = curQM->label[i++];
    buffer[5] = curQM->label[i++];
    buffer[6] = curQM->label[i++];
    buffer[7] = curQM->label[i];
}


/* 
 * getByteMemTaint(uint64_t addr, uint32_t size, uint8_t *buf) -- 
 * Retrieves size bytes from the address at addr in shadow memory and stores 
 * them into the buffer at buf.  Assumes buf is at least size long.
 */
void getByteMemTaint(ByteMemTaint *state,
		     ADDRINT addr,
		     uint32_t sizeInBytes,
		     uint64_t * buffer) {
    if (sizeInBytes <= 8) {
        if (sizeInBytes == 4) {
            load32(state, addr, buffer);
            return;
        }
        else if (sizeInBytes == 8) {
            load64(state, addr, buffer);
            return;
        }
        else if (sizeInBytes == 2) {
            load16(state, addr, buffer);
            return;
        }
        else if (sizeInBytes == 1) {
            load8(state, addr, buffer);
            return;
        }
    }

    uint32_t i = 0;

    while (sizeInBytes != 0 && isNotAligned (addr, 8)) {
        load8(state, addr++, buffer + i);
        i++;
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        load64(state, addr, (buffer + i));
        i += 8;
        addr += 8;
        sizeInBytes -= 8;
    }

    while (sizeInBytes > 0) {
        load8(state, addr++, buffer + i);
        i++;
        sizeInBytes--;
    }
}


/*
 * add8(state, labelState, addr, label) -- adds label to the taint label
 * to the taint label at the byte at location addr.
 */
static inline void add8(ByteMemTaint *state,
			LabelStoreState *labelState,
			ADDRINT addr,
			uint64_t label) {
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = (addr & ADDR_MASK);
    applyLabel(labelState, label, curQM->label + i, 1);
}


/*
 * addSlow(state, labelState, addr, label, sizeInBytes) -- adds label to
 * the taint for sizeInBytes locations, starting at location addr, one
 * byte at a time.  The slowest way to store large values.  
 */
static inline void addSlow(ByteMemTaint *state,
			   LabelStoreState *labelState,
			   ADDRINT addr,
			   uint64_t label,
			   uint32_t sizeInBytes) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        add8(state, labelState, addr + i, label);
    }
}


/*
 * add16/add32/add64(ADDRINT, addr, uint8_t *val) -- Quickly adds label to
 * the taint labels for 2/4/8 bytes from val into shadow memory starting 
 * at address addr.
 */
static inline void add16(ByteMemTaint *state,
			 LabelStoreState *labelState,
			 ADDRINT addr,
			 uint64_t label) {
    if (isNotAligned(addr, 2)) {
        addSlow(state, labelState, addr, label, 2);
        return;
    }

    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    applyLabel(labelState, label, curQM->label + i, 2);
}

static inline void add32(ByteMemTaint *state,
			 LabelStoreState *labelState,
			 ADDRINT addr,
			 uint64_t label) {
    if (isNotAligned(addr, 4)) {
        addSlow(state, labelState, addr, label, 4);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    applyLabel(labelState, label, curQM->label + i, 4);
}

static inline void add64(ByteMemTaint *state,
			 LabelStoreState *labelState,
			 ADDRINT addr,
			 uint64_t label) {
    if (isNotAligned(addr, 8)) {
        addSlow(state, labelState, addr, label, 8);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    applyLabel(labelState, label, curQM->label + i, 8);
}


/*
 * addTaintToByteMem(uint64_t addr, uint32_t sizeInBytes, uint8_t *val) --
 * Adds label to the taint label for sizeInBytes locations starting at
 * address addr. 
 */
void addTaintToByteMem(ByteMemTaint *state,
		       LabelStoreState *labelState,
		       ADDRINT addr,
		       uint32_t sizeInBytes,
		       uint64_t label) {
    if (sizeInBytes <= 8) {
        if (sizeInBytes == 4) {
            add32(state, labelState, addr, label);
            return;
        }
        else if (sizeInBytes == 8) {
            add64(state, labelState, addr, label);
            return;
        }
        else if (sizeInBytes == 2) {
            add16(state, labelState, addr, label);
            return;
        }
        else if (sizeInBytes == 1) {
            add8(state, labelState, addr, label);
            return;
        }
    }

    while (sizeInBytes != 0 && isNotAligned(addr, 8)) {
        add8(state, labelState, addr, label);
        addr++;
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        add64(state, labelState, addr, label);
        sizeInBytes -= 8;
        addr += 8;
    }

    while (sizeInBytes > 0) {
        add8(state, labelState, addr, label);
        addr++;
        sizeInBytes--;
    }
}


/*
 * combine8() -- returns the taint label obtained by combining curLabel 
 * with the taint label at the byte at location addr.
 */
static inline uint64_t combine8(ByteMemTaint *state,
				LabelStoreState *labelState,
				ADDRINT addr,
				uint64_t curLabel) {
    QM *curQM = getQMForReading(state, addr);
    return combineLabels(labelState, curQM->label[addr & ADDR_MASK], curLabel);
}


/*
 * combineSlow() -- returns the taint label obtained by combining curLabel
 * with the taint labels for sizeInBytes bytes starting at addr.
 */
static inline uint64_t combineSlow(ByteMemTaint *state,
				   LabelStoreState *labelState,
				   ADDRINT addr,
				   uint32_t sizeInBytes,
				   uint64_t curLabel) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        curLabel = combine8(state, labelState, addr + i, curLabel);
    }

    return curLabel;
}


/*
 * combine16/combine32/combine64() -- Quickly combines the taint label curLabel
 * with the taint labels for 2/4/8 bytes at address addr.
 */
static inline uint64_t combine16(ByteMemTaint *state,
				 LabelStoreState *labelState,
				 ADDRINT addr,
				 uint64_t curLabel) {
    if (isNotAligned(addr, 2)) {
        return combineSlow(state, labelState, addr, 2, curLabel);
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;

    uint64_t lastLabel = 0;
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }

    if(lastLabel != curQM->label[i]) {
        return combineLabels(labelState, curQM->label[i], curLabel);
    }
    return curLabel;
}

static inline uint64_t combine32(ByteMemTaint *state,
				 LabelStoreState *labelState,
				 ADDRINT addr,
				 uint64_t curLabel) {
    if (isNotAligned(addr, 4)) {
        return combineSlow(state, labelState, addr, 4, curLabel);
    }

    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;

    uint64_t lastLabel = 0;
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }

    if(lastLabel != curQM->label[i]) {
        return combineLabels(labelState, curQM->label[i], curLabel);
    }
    return curLabel;
}

static inline uint64_t combine64(ByteMemTaint *state,
				 LabelStoreState *labelState,
				 ADDRINT addr,
				 uint64_t curLabel) {
    if (isNotAligned(addr, 8)) {
        return combineSlow(state, labelState, addr, 8, curLabel);
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;

    uint64_t lastLabel = 0;
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        lastLabel = curQM->label[i++];
        curLabel = combineLabels(labelState, lastLabel, curLabel);
    }
    if(lastLabel != curQM->label[i]) {
        return combineLabels(labelState, curQM->label[i], curLabel);
    }
    return curLabel;
}


/*
 * getCombinedByteMemTaint() -- returns the taint label obtained by
 * combining curLabel with the taint labels for sizeInBytes bytes of memory
 * starting at addr.
 */
uint64_t getCombinedByteMemTaint(ByteMemTaint *state,
				 LabelStoreState *labelState,
				 uint64_t addr,
				 uint32_t sizeInBytes,
				 uint64_t curLabel) {
    if (sizeInBytes <= 8) {
        if (sizeInBytes == 4) {
            return combine32(state, labelState, addr, curLabel);
        }
        else if (sizeInBytes == 8) {
            return combine64(state, labelState, addr, curLabel);
        }
        else if (sizeInBytes == 2) {
            return combine16(state, labelState, addr, curLabel);
        }
        else if (sizeInBytes == 1) {
            return combine8(state, labelState, addr, curLabel);
        }
    }

    while (sizeInBytes != 0 && isNotAligned (addr, 8)) {
        curLabel = combine8(state, labelState, addr++, curLabel);
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        curLabel = combine64(state, labelState, addr, curLabel);
        addr += 8;
        sizeInBytes -= 8;
    }

    while (sizeInBytes > 0) {
        curLabel = combine8(state, labelState, addr++, curLabel);
        sizeInBytes--;
    }
    
    return curLabel;
}
