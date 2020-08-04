/*
* File: ShadowMemory.c
* Authors: Jon Stephens and David Raicher
* Description: This provides a Shadow Memory backend for the trace reader.  It does this implementing a quad map structure to provide byte level
* addressing for memory reads and writes.  The main interfaces for interacting with the Shadow Memory are the store and load functions.  If you are
* doing development on this code please note that there are 1/2/4/8 bit versions of store/load internally to allow for faster reads/writes.
*/

#include "ShadowMemory.h"
#include "ReaderUtils.h"
#include <strings.h>

/**
 * Data Structure!
 * So the structure here can be a bit cryptic, especially since they are defined all over the place, I'll
 * provide a description here to hopefully make things a little easier. There are 4 levels to this structure
 * each accessed by a portion of the address. You can think of it as a hash map with 4 levels. You access
 * the first using the most significant quarter of the address to get to the second hash map, then so on
 * with the third and fourth so that the fourth hash map is accessed with the least significant quarter of 
 * the address. Each has map has a "default value" defined by a D in front of the level it is the default
 * value for. Therefore DSM is the default value for the SM (secondary map). The default values can be read
 * from, but not written to and are initialized such that all memory values are 0. Therefore we have the 
 * following:
 * 
 * PM: Top most level of the hash map stored within the MemState. Points to a SM
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
#define MOD_SIZE LVL_SIZE >> 3

typedef struct QM_t {
    uint8_t data[LVL_SIZE];
    uint8_t mod[MOD_SIZE];
    
} QM;

struct MemState_t {
    QM ***PM[LVL_SIZE];        // default secondary map
};

static QM ***DSM = NULL;            // 256
static QM **DTM = NULL;
static QM *DQM = NULL;
static int refCnt = 0;

/*
* initShadowMemory() -- Initializes the Shadow Memory.  This must be called before any other Shadow Memory functions are called.
* freeShadowMemory should also be called before any subsequent calls to this function to prevent memory leakage.
*/

MemState *initShadowMemory() {
    MemState *state = malloc(sizeof(MemState));
    if (state == NULL) {
        throwError("Could not allocate space for memory state");
    }

    char init = 0;
    if (DSM == NULL) {
        DSM = malloc(sizeof (QM **[LVL_SIZE]));
        init = 1;
    }

    if (DTM == NULL) {
        DTM = malloc(sizeof (QM *[LVL_SIZE]));
        init = 1;
    }

    if (DQM == NULL) {
        DQM = calloc(sizeof (QM), 1);
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
* reset() -- Resets the state of the shadow memory to be completely clean (that is, just like after a call to initShadowMemory()). 
*/

void reset(MemState *state) {
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

void freeGlobalState() {
    free(DSM);
    free(DTM);
    free(DQM);
}

/*
* freeShadowMemory() -- Frees all memory allocated by the Shadow Memory.
*/

void freeShadowMemory(MemState *state) {
    reset(state);
    free(state);
    refCnt--;
    if (refCnt == 0) {
        freeGlobalState();
    }
}

static inline SM getSM(MemState *state, ADDRINT addr) {
    return state->PM[addr >> PM_SHAMT];
}

static inline TM getTM(ADDRINT addr, SM curSM) {
    return curSM[(addr >> SM_SHAMT) & ADDR_MASK];
}

static inline QM *getQM(ADDRINT addr, TM curTM) {
    return curTM[(addr >> TM_SHAMT) & ADDR_MASK];
}

static inline SM *getSMPtr(MemState *state, ADDRINT addr) {
    return &state->PM[addr >> PM_SHAMT];
}

static inline TM *getTMPtr(ADDRINT addr, SM curSM) {
    return &curSM[(addr >> SM_SHAMT) & ADDR_MASK];
}

static inline QM **getQMPtr(ADDRINT addr, TM curTM) {
    return &curTM[(addr >> TM_SHAMT) & ADDR_MASK];
}

/*
* copyForWritingTM(ADDRINT addr, TM *srcTM) -- Replaces TM in primary map, and returns
* created QM.
*/

static inline QM *copyForWritingTM(ADDRINT addr, TM * srcTM) {
    TM newTM = malloc(sizeof (QM *[LVL_SIZE]));

    if (newTM == NULL) {
        throwError("Unable to allocate space for new TM");
    }

    memcpy(newTM, *srcTM, sizeof (QM *[LVL_SIZE]));

    QM *newQM = malloc(sizeof (QM));

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
* copyForWritingSM(ADDRINT addr, TM *srcTM) -- Replaces SM in primary map, and returns
* created QM.
*/

static inline QM *copyForWritingSM(ADDRINT addr, SM * srcSM) {
    SM newSM = malloc(sizeof (QM **[LVL_SIZE]));

    if (newSM == NULL) {
        throwError("Unable to allocate space for new SM");
    }

    memcpy(newSM, *srcSM, sizeof (QM **[LVL_SIZE]));

    TM newTM = malloc(sizeof (QM *[LVL_SIZE]));

    if (newTM == NULL) {
        throwError("Unable to allocate space for new TM");
    }

    TM *curTM = getTMPtr(addr, newSM);
    memcpy(newTM, *curTM, sizeof (QM *[LVL_SIZE]));

    QM *newQM = malloc(sizeof (QM));

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
* copyForWritingSM(ADDRINT addr, TM *srcTM) -- Replaces QM in primary map, and returns
* created QM.
*/

static inline QM *copyForWritingQM(ADDRINT addr, QM ** srcQM) {
    QM *newQM = malloc(sizeof (QM));

    if (newQM == NULL) {
        throwError("Unable to allocate space for new QM");
    }

    memcpy(newQM, *srcQM, sizeof (QM));

    *srcQM = newQM;

    return newQM;
}

/*
* getQMForReading(ADDRINT addr) -- Returns the QM at the specified address for reading.
*/

static inline QM *getQMForReading(MemState *state, ADDRINT addr) {
    return state->PM[addr >> PM_SHAMT][(addr >> SM_SHAMT) & ADDR_MASK][(addr >> TM_SHAMT) & ADDR_MASK];
}

/*
* getQMForWriting(ADDRINT addr) -- Returns the QM for the specified address.
*/

static inline QM *getQMForWriting(MemState *state, ADDRINT addr) {
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
* isDefault() -- Checks to see which chunks of memory have been allocated (and hence are no longer the default map values).
*/

void isDefault(MemState *state) {
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
* chkDefaultVals() -- Checks to ensure the default map values are consistent.
*/

void chkDefaultVals(void) {
    uint32_t i;
    for (i = 0; i < LVL_SIZE; i++) {
        if (DSM != NULL && DSM[i] != DTM) {
            printf("Bad DSM at %d\n", i);
        }
        if (DTM != NULL && DTM[i] != DQM) {
            printf("Bad DTM at %d\n", i);
        }
        if (i < LVL_SIZE && DQM->data[i] != 0 && DQM->mod[i % MOD_SIZE] != 0) {
            printf("Bad DQM at %p, val: %x\n", DQM, DQM->data[i]);
        }
    }
}

/*
* isNotAligned(ADDRINT addr, uint32_t alignment) -- Checks to see if the size of a given read/write is aligned with
* the QM size.  Easy check to see if a write/read may be split between seperate QMs.
*/

static inline bool isNotAligned(ADDRINT addr, uint32_t alignment) {
    return addr & (alignment - 1);
}

/*
* hasSpace(ADDRINT addr, unit32_t sizeInBytes) -- Determines if there is space on the given QM starting at addr for the
* given write.  Can be used to ensure there isn't a write off the side of the array. Returns 0 if there is not space.  1 otherwise.
*/

static inline bool hasSpace(ADDRINT addr, uint32_t sizeInBytes) {
    return ((addr & ADDR_MASK) + sizeInBytes) < ADDR_MASK;
}

/*
* store8(ADDRINT addr, uint8_t newVal) -- Stores newVal (a byte) at the given address.
*/

static inline void store8(MemState *state, ADDRINT addr, uint8_t newVal) {
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = (addr & ADDR_MASK);
    curQM->data[i] = newVal;
    curQM->mod[i >> 3] |= (0x1 << (i % 8));
}

/*
* storeSlow(ADDRINT addr, uint8_t *val, uint32_t sizeInBytes) -- Stores the bytes in the buffer val starting at addr one by one.  Obviously
* the slowest way to store large values.  Assume val is at least sizeInBytes long.
*/

static void storeSlow(MemState *state, ADDRINT addr, uint8_t * val, uint32_t sizeInBytes) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        store8(state, addr + i, *(val + i));
    }
}

/*
* store16/store32/store64(ADDRINT, addr, uint8_t *val) -- Quickly stores 2/4/8 bytes from val into shadow memory starting at address addr.  Assumes
* That val is long enough for the write.
*/

static inline void store16(MemState *state, ADDRINT addr, uint8_t * val) {
    if (isNotAligned(addr, 2)) {
        storeSlow(state, addr, val, 2);
        return;
    }

    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *srcBuf = curQM->data + i;
    *((uint16_t *) srcBuf) = *((uint16_t *) val);
    //aligned, so values of i can only be 0, 2 or 4
    curQM->mod[i >> 3] |= (0x3 << (i % 8));
}

static inline void store32(MemState *state, ADDRINT addr, uint8_t * val) {
    if (isNotAligned(addr, 4)) {
        storeSlow(state, addr, val, 4);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *srcBuf = curQM->data + i;
    *((uint32_t *) srcBuf) = *((uint32_t *) val);
    curQM->mod[i >> 3] |= (0xf << (i % 8));
}

static inline void store64(MemState *state, ADDRINT addr, uint8_t * val) {
    if (isNotAligned(addr, 8)) {
        storeSlow(state, addr, val, 8);
        return;
    }
    QM *curQM = getQMForWriting(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *srcBuf = curQM->data + i;
    *((uint64_t *) srcBuf) = *((uint64_t *) val);
    curQM->mod[i >> 3] |= 0xff;
}

/*
* store(uint64_t addr, uint32_t sizeInBytes, uint8_t *val) --Stores the series of bytes (up to sizeInBytes) stored in val into the shadow 
* memory at address addr.  Assumes val is at least sizeInBytes long.
*/

void store(MemState *state, ADDRINT addr, uint32_t sizeInBytes, uint8_t * val) {
    if (sizeInBytes <= 8) {
        if (sizeInBytes == 4) {
            store32(state, addr, val);
            return;
        }
        else if (sizeInBytes == 8) {
            store64(state, addr, val);
            return;
        }
        else if (sizeInBytes == 2) {
            store16(state, addr, val);
            return;
        }
        else if (sizeInBytes == 1) {
            store8(state, addr, *val);
            return;
        }
    }

    while (sizeInBytes != 0 && isNotAligned(addr, 8)) {
        store8(state, addr, *val);
        addr++;
        val++;
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        store64(state, addr, val);
        sizeInBytes -= 8;
        val += 8;
        addr += 8;
    }

    while (sizeInBytes > 0) {
        store8(state, addr, *val);
        addr++;
        val++;
        sizeInBytes--;
    }
}

/*
* load8(ADDRINT addr) -- Returns the value currently stored at addr (1 byte).
*/

static inline uint8_t load8(MemState *state, ADDRINT addr) {
    QM *curQM = getQMForReading(state, addr);
    uint8_t retVal = curQM->data[addr & ADDR_MASK];
    return retVal;
}

/*
* loadSlow(ADDRINT addr, uint32_t sizeInBytes, uint8_t *buffer) -- Loads sizeInBytes bytes into buffer starting at addr.  This is done
* bytewise and hence is the slowest possible way.  Assumes buffer is at least sizeInBytes long.
*/

static void loadSlow(MemState *state, ADDRINT addr, uint32_t sizeInBytes, uint8_t *buffer) {
    uint32_t i;
    for (i = 0; i < sizeInBytes; i++) {
        *(buffer + i) = load8(state, addr + i);
    }
}

/*
* load16/load32/load64(ADDRINT, addr, uint8_t *buffer) -- Quickly loads 2/4/8 bytes from shadow memory into buffer starting at address addr.  Assumes
* That buffer is sizeInBytes long.
*/

static inline void load16(MemState *state, ADDRINT addr, uint8_t * buffer) {
    if (isNotAligned(addr, 2)) {
        loadSlow(state, addr, 2, buffer);
        return;
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *destBuf = curQM->data + i;
    *((uint16_t *) buffer) = *((uint16_t *) destBuf);
}

static inline void load32(MemState *state, ADDRINT addr, uint8_t * buffer) {
    if (isNotAligned(addr, 4)) {
        loadSlow(state, addr, 4, buffer);
        return;
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *destBuf = curQM->data + i;
    *((uint32_t *) buffer) = *((uint32_t *) destBuf);
}

static inline void load64(MemState *state, ADDRINT addr, uint8_t * buffer) {
    if (isNotAligned(addr, 8)) {
        loadSlow(state, addr, 8, buffer);
        return;
    }
    QM *curQM = getQMForReading(state, addr);
    uint32_t i = addr & ADDR_MASK;
    uint8_t *destBuf = curQM->data + i;
    *((uint64_t *) buffer) = *((uint64_t *) destBuf);
}

/* 
* load(uint64_t addr, uint32_t size, uint8_t *buf) -- Retrieves size bytes from the address at addr in shadow memory 
* and stores them into the buffer at buf. Assumes buf is at least size long.
*/

void load(MemState *state, ADDRINT addr, uint32_t sizeInBytes, uint8_t * buffer) {
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
            buffer[0] = load8(state, addr);
            return;
        }
    }

    uint32_t i = 0;

    while (sizeInBytes != 0 && isNotAligned (addr, 8)) {
        buffer[i++] = load8(state, addr++);
        sizeInBytes--;
    }

    while (sizeInBytes >= 8) {
        load64(state, addr, (buffer + i));
        i += 8;
        addr += 8;
        sizeInBytes -= 8;
    }

    while (sizeInBytes > 0) {
        buffer[i++] = load8(state, addr++);
        sizeInBytes--;
    }
}

const uint8_t *getNextMemRegion(MemState *state, uint64_t *addr, uint32_t *size) {
    uint64_t start = *addr + *size;
    uint8_t *data = NULL;

    uint64_t i = (start >> PM_SHAMT);
    uint64_t j = (start >> SM_SHAMT) & ADDR_MASK;
    uint64_t k = (start >> TM_SHAMT) & ADDR_MASK;
    uint64_t l = start & ADDR_MASK;
    for (; i < LVL_SIZE; i++) {
        SM curSM = state->PM[i];
        if (!is_DSM(curSM)) {
            for (; j < LVL_SIZE; j++) {
                TM curTM = curSM[j];
                if (!is_DTM(curTM)) {
                    for (; k < LVL_SIZE; k++) {
                        QM *curQM = curTM[k];
                        if (!is_DQM(curQM)) {
                            uint64_t m;
                            uint8_t startingByte = l % 8;
                            for(m = l >> 3; m < MOD_SIZE; m++) {
                                if(data == NULL) {
                                    uint8_t curByte = (startingByte == 0) ? curQM->mod[m] : (curQM->mod[m] & ~((1 << startingByte) - 1));
                                    if(curByte != 0) {
                                        startingByte = ffs(curByte) - 1;
                                        uint64_t ind = (m << 3) | startingByte;
                                        *addr = (i << PM_SHAMT) | (j << SM_SHAMT) | (k << TM_SHAMT) | ind;
                                        data = curQM->data + ind;
                                    }
                                }
                                if(data != NULL) {
                                    uint8_t curByte = (startingByte == 0) ? curQM->mod[m] : (curQM->mod[m] | ((1 << startingByte) - 1));
                                    if(curByte != 0xff) {
                                        uint64_t regionEnd = (i << PM_SHAMT) | (j << SM_SHAMT) | (k << TM_SHAMT) | (m << 3) | (ffs(~curByte) - 1);
                                        *size = regionEnd - *addr;
                                        return data;
                                    }
                                }
                                startingByte = 0;
                            }

                            //off of page
                            if(data != NULL) {
                                uint64_t regionEnd = (i << PM_SHAMT) | (j << SM_SHAMT) | ((k+1) << TM_SHAMT);
                                *size = regionEnd - *addr;
                                return data;
                            }
                        }
                        l = 0;
                    }
                }

                k = 0;
                l = 0;
            }
        }

        j = 0;
        k = 0;
        l = 0;
    }

    return data;
}
