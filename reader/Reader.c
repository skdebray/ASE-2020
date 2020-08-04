/**
 * This file contains the implementation of all the functions in the Reader's public API. Additionally, it 
 *  includes all of the state necessary to support the public API and any helper functions that are only
 *  needed by the public API (such as reading the sections of the trace required by any formats). 
 **/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include <XedLynxReg.h>
#include "ReaderUtils.h"
#include "Reader.h"
#include "ShadowMemory.h"
#include "ShadowRegisters.h"
#include "DataOpsReader.h"
#include "XedDisassembler.h"

/**
 * Function: readHeader
 * Description: Reads the fileHeader from the given file and places the information within header.
 * Returns: 1 if successful, 0 otherwise
 **/
uint32_t readHeader(FILE * trace, FileHeader * header) {
    uint32_t retVal = 0;
    if (fseek(trace, 0, SEEK_SET)) {
        throwError("Couldn't seek to file header location");;
    }

    retVal = fread(header, sizeof (FileHeader), 1, trace);

    return retVal;
}

/**
 * Function: readSectionTable
 * Description: readSectionTable reads the section table from the given file.  Assumes that the 
 *  fileHeader has been read in already.  The value returned represents the number of section 
 *  entries read.  Note that an array is dynamically allocated at the address pointed to by 
 *  sectionTable. This must be freed by the caller. 
 * Returns: Number of sectoins read
 **/
uint32_t readSectionTable(FILE * trace, FileHeader *header, int numEntries, SectionEntry *table) {
    if (fseek (trace, header->sectionTableOff, SEEK_SET)) {
        throwError("Couldn't seek to section table location");
    }

    uint32_t readAmt = fread(table, header->sectionEntrySize, numEntries, trace);

    return readAmt;
}

/**
 * Function: fetchStrFromId
 * Description: Retrieves string using the given id
 * Assumptions: The ReaderState and ReaderIns are valid
 * Returns: The string
 **/
const char *fetchStrFromId(ReaderState *state, uint32_t id) {
    return state->fetchStr(state->formatState, id);
}

/**
 * Function: findString
 * Description: Finds the id for the given string
 * Assumptions: The ReaderState and ReaderIns are valid
 * Returns: The corresponding id or -1 if not found
 **/
uint32_t findString(ReaderState *state, const char *str) {
    return state->findStr(state->formatState, str);
}

/**
 * Function: getCurEid
 * Description: Gets the unique event id for the current instruction
 * Assumptions: ReaderState is valid
 * Returns: Current event id
 **/
uint64_t getCurEid(ReaderState *state) {
    return state->curId;
}

/**
 * Function: nextEvent
 * Description: Goes to the next instruction in the trace. This is left up to the reader format, but will
 *  involve updating the shadow state and filling in the input event.
 * Assumptions: ReaderState is valid
 * Returns: 1 if successful, 0 otherwise 
 **/
uint32_t nextEvent(ReaderState *state, ReaderEvent *event) {
    return nextEventWithCheck(state, event, NULL);
}

/**
 * Function: nextEventWithCheck
 * Description: Similar to nextEvent, but also passes extra information about the previous instruction
 *  so that the readerFormat can perform extra checks to see if the internal state reflects the expected
 *  state.
 * Assumptions: ReaderState is valid and InsInfo is the InsInfo for the instruction returned by the last
 *  call to nextEvent or nextEventWithCheck
 * Returns: 1 if successful, 0 otherwise
 **/
uint32_t nextEventWithCheck(ReaderState *state, ReaderEvent *event, InsInfo *info) {
    if (event == NULL) {
        throwError("Cannot pass a NULL event to nextReaderEvent");
    }

    return state->readEvent(state->formatState, state->memState, state->regState, 
        state->disState, event, state->curId++, info);
}

/**
 * Function: initReader
 * Description: Sets up the internal state of the reader so that a trace can be process. This involves
 *  reading trace headers and setting state appropriately (such as setting up architectural elements).
 * Returns: ReaderState for the given trace
 **/
ReaderState *initReader(char *fileName, uint32_t debugFlag) {
    ReaderState *state = malloc(sizeof(ReaderState));
    if (state == NULL) {
        throwError("couldn't allocate reader state");
    }
    //we set curId to -1 since nextEvent will increment
    state->curId = 0;

    //make sure to opoen as a binary file
    state->trace = fopen(fileName, "rb");

    if (state->trace == NULL) {
        free(state);
        throwError("Unable to open specified file");
    }

    state->debug = debugFlag;

    //read the traces file header
    if (readHeader(state->trace, &state->fileHeader) != 1) {
        free(state);
        throwError("Unable to read trace header");
    }
    if(strncmp((char *)&state->fileHeader.ident, "UATRC", 5) != 0 || state->fileHeader.ident[5] != 2) {
        free(state);
        throwError("Unsupported trace version");
    }

    //perform architecture-dependent setup
    switch (state->fileHeader.archType) {
        case X86_ARCH:
            state->disState = setupXed_x86();
            state->getLynxRegSize = LynxRegSize32;
            state->getFullLynxReg = LynxReg2FullLynxIA32EReg;
            break;
        case X86_64_ARCH:
            state->disState = setupXed_x64();
            state->getLynxRegSize = LynxRegSize;
            state->getFullLynxReg = LynxReg2FullLynxReg;
            break;
        default:
            free(state);
            throwError("Unknown architecture");
    }

    //setup state based on the trace type
    FormatState (*initFormat) (FileHeader *, SectionEntry *, FILE *, ExInfo *); 
    void (*setupInitialState) (FormatState, MemState *, RegState *);
    switch (state->fileHeader.traceType) {
        case DATA_OPS_TRACE:
            initFormat = (FormatState (*)(FileHeader *, SectionEntry *, FILE *, ExInfo *)) initDataOps;
            setupInitialState = (void (*)(FormatState, MemState *, RegState *)) setupDataOpsInitState;
            state->readEvent = (uint32_t (*) (FormatState, MemState *, RegState *, DisassemblerState *, 
                ReaderEvent *, uint64_t, InsInfo *)) readDataOpsEvent;
            state->freeFormat = (void (*) (FormatState)) freeDataOps;
            state->fetchStr = (const char *(*) (FormatState, uint32_t)) fetchDataOpsStr;
            state->findStr = (uint32_t (*) (FormatState, const char *)) findDataOpsStr;
            state->hasFields = (uint8_t (*) (FormatState, uint32_t)) hasDataOpsField;
            state->strTableSize = (uint32_t (*) (FormatState)) dataOpsStrTableSize;
            break;
        default:
            free(state);
            throwError("Unsupported trace type");
    }

    //read the section table
    SectionEntry *sectionTable = malloc(state->fileHeader.sectionNumEntry * state->fileHeader.sectionEntrySize);

    if (sectionTable == NULL) {
        free(state);
        throwError("Could not allocate memory for section table");
    }

    if (readSectionTable(state->trace, &state->fileHeader, state->fileHeader.sectionNumEntry, sectionTable) 
            != state->fileHeader.sectionNumEntry) {
        free(sectionTable);
        free(state);
        throwError("Could not read section table");
    }

    //allow the selected format to do any necessary setup
    state->formatState = initFormat(&state->fileHeader, sectionTable, state->trace, &state->exInfo);

    //setup the shadow architecture
    state->regState = initShadowRegisters(state->exInfo.numThreads);
    state->memState = initShadowMemory();

    //allow the format to initialize the shadow architecture
    setupInitialState(state->formatState, state->memState, state->regState);

    //we no longer need the section table
    free (sectionTable);

    return state;
}

/**
 * Function: initInsInfo
 * Description: Sets up the InsInfo struct for use. This is because part of the memory used to store 
 *  operands is statically allocated and the other part is dynamically allocated in an attempt to
 *  reduce the number of dynamic allocations.
 * Returns: None
 **/
void initInsInfo(InsInfo *info) {
    int i;

    //srcOps, dstOps and readWriteOps are all partially statically defined linked lists, so link them 
    // appropriately
    for(i = 0; i < 2; i++) {
        info->srcOps[i].type = NONE_OP;
        info->srcOps[i].next = &info->srcOps[i + 1];
    }

    info->srcOpCnt = 0;
    info->srcOps[2].type = NONE_OP;
    info->srcOps[2].next = NULL;

    info->dstOpCnt = 0;
    info->dstOps[0].type = NONE_OP;
    info->dstOps[0].next = NULL;

    info->readWriteOpCnt = 0;
    info->readWriteOps[0].type = NONE_OP;
    info->readWriteOps[0].next = NULL;
}

/**
 * Function: freeReaderOpList
 * Description: Frees a ReaderOp linked list recursively. Note, while this is not typically a good idea 
 *  because you can run out of stack space, we know that the lists will be small.
 * Returns: None
 **/
void freeReaderOpList(ReaderOp *op) {
    if (op == NULL) {
        return;
    }

    freeReaderOpList(op->next);
    free(op);
}

/**
 * Function: freeReaderOpList
 * Description: Frees up the additional dynamically allocated space from the InsInfo Operand fields
 * Returns: None
 **/
void freeInsInfo(InsInfo *info) {
    freeReaderOpList(info->srcOps[2].next);
    freeReaderOpList(info->dstOps[0].next);
    freeReaderOpList(info->readWriteOps[0].next);
}

/**
 * Function: fetchInsInfo
 * Description: Fetches additional information about an instruction from XED.
 * Returns: None
 **/
void fetchInsInfo(ReaderState *state, ReaderIns *ins, InsInfo *info) {
    //reset counts, but note the linked list does NOT get modified, that way we can take advantage of 
    // past allocations
    info->srcOpCnt = 0;
    info->dstOpCnt = 0;
    info->readWriteOpCnt = 0;

    xed_decoded_inst_t xedIns;
    decodeIns(state->disState, &xedIns, ins->binary, 15);
    getInsFlags(&xedIns, &info->srcFlags, &info->dstFlags);
    info->insClass = getInsClass(&xedIns);
    getInsMnemonic(&xedIns, info->mnemonic, 128);
    getInsOps(state->disState, state->regState, &xedIns, ins->tid, info->srcOps, &info->srcOpCnt, 
        info->dstOps, &info->dstOpCnt, info->readWriteOps, &info->readWriteOpCnt);
}

/** 
 * Function: CloseReader
 * Description: Closes and frees a ReaderState. Note, after this occurs you cannot access any of the
 *  state from that trace so only do this once you're sure you are done processing everything.
 * Returns: None
 **/
void closeReader(ReaderState *state) {
    if (state->trace != NULL) {
        fclose(state->trace);
    }

    if(state->exInfo.loadSegments != NULL) {
        free(state->exInfo.loadSegments);
    }
    state->freeFormat(state->formatState);
    freeShadowMemory(state->memState);
    freeShadowRegisters(state->regState);
    closeXed(state->disState);
    free(state);
}

/**
 * Function: getMemoryVal
 * Description: Retrieves a memory value from the shadow state and puts it in the given buffer
 * Returns: None
 **/
void getMemoryVal(ReaderState *state, uint64_t addr, uint32_t size, uint8_t * buf) {
    load(state->memState, addr, size, buf);
}

/**
 * Function: getRegisterVal
 * Description: Retrieves and returns a register value from the shadow state
 * Returns: The register value
 **/
const uint8_t *getRegisterVal(ReaderState *state, LynxReg reg, uint32_t thread) {
    return getThreadVal(state->regState, reg, thread);
}

/**
 * Function: getNumThreads
 * Description: Returns the total number of threads used in the trace
 * Returns: Number of threads
 **/
uint32_t getNumThreads(ReaderState *state) {
    return state->exInfo.numThreads;
}

/**
 * Function: loadedFromSegment
 * Description: Determines if a given address was loaded from the executable at runtime. If it was, the
 *  segment's flags are returned (i.e. can check if the segment was readable, writable, executable)
 * Returns: Segment's flags if in a segment, 0 otherwise
 **/
uint32_t loadedFromSegment(ReaderState *state, uint64_t addr) {
    ExInfo *exInfo = &state->exInfo;
    uint32_t i;
    for(i = 0; i < exInfo->numSegments; i++) {
        if (addr >= exInfo->loadSegments[i].addr && 
                addr < (exInfo->loadSegments[i].addr + exInfo->loadSegments[i].size)) {
            return exInfo->loadSegments[i].flags;
        }
    }

    return 0;
}

uint32_t numSegments(ReaderState *state) {
    return state->exInfo.numSegments;
}

const SegmentLoad *getSegment(ReaderState *state, int i) {
    return state->exInfo.loadSegments + i;
}

/**
 * Function: hasFields
 * Description: Traces aren't required to have every field within the trace, they can be turned off to save
 *  space. This function will check if all the flags given by query are in the trace. Note, use getSelMask
 *  on the DataSelect enumeration within TraceFileHeader.h to get the mask.
 * Returns: 1 if all there, 0 otherwise
 **/
uint8_t hasFields(ReaderState *state, uint32_t query) {
    return state->hasFields(state->formatState, query);
}

/**
 * Function: nextOp
 * Description: This is an iterator function for the ReaderOp list since people commonly access it 
 *  incorrectly. It will fetch the next operand and will return NULL once the end of the list has 
 *  been reached
 * Returns: pointer to the next ReaderOp, or NULL upon reaching the end of the list
 **/
ReaderOp *nextOp(ReaderOp *curOp, uint8_t totalOpCnt, uint8_t curIt) {
    if (curIt < totalOpCnt) {
        return curOp->next;
    }

    return NULL;
}

/**
 * Function: getMemReads
 * Description: This gets all of the memory reads contained within the InsInfo and places them into the 
 *  readOps array. The array is assumed to be large enough, so either pass in an array of size 32 (there 
 *  will never be this many memory reads), or (info->srcOpCnt + info->readWriteOpCnt).
 * Returns: Number of memory reads in info
 **/
uint8_t getMemReads(InsInfo *info, ReaderOp **readOps) {
    int pos = 0;

    int i;
    ReaderOp *curOp = info->srcOps;
    for(i = 0; i < info->srcOpCnt; i++) {
        if (curOp->type == MEM_OP) {
            readOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    curOp = info->readWriteOps;
    for(i = 0; i < info->readWriteOpCnt; i++) {
        if (curOp->type == MEM_OP) {
            readOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    return pos;
}

/**
 * Function: getMemWrites
 * Description: This gets all of the memory writes contained within the InsInfo and places them into the 
 *  writeOps array. The array is assumed to be large enough, so either pass in an array of size 32 (there 
 *  will never be this many memory writes), or (info->dstOpCnt + info->readWriteOpCnt).
 * Returns: Number of memory writes in info
 **/
uint8_t getMemWrites(InsInfo *info, ReaderOp **writeOps) {
    int pos = 0;

    int i;
    ReaderOp *curOp = info->readWriteOps;
    for(i = 0; i < info->readWriteOpCnt; i++) {
        if (curOp->type == MEM_OP) {
            writeOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    curOp = info->dstOps;
    for(i = 0; i < info->dstOpCnt; i++) {
        if (curOp->type == MEM_OP) {
            writeOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    return pos; 
}

ArchType getArchType(ReaderState *state) {
    return getXedArchType(state->disState);
}

uint8_t getAddrSize(ReaderState *state) {
    switch(getXedArchType(state->disState)) {
        case X86_ARCH:
            return 4;
        case X86_64_ARCH:
            return 8;
        default:
            throwError("Arch missing from getAddrSize");
    }

    return 0;
}

uint32_t getRegSize(ReaderState *state, LynxReg reg) {
    return state->getLynxRegSize(reg);
}

LynxReg getFullReg(ReaderState *state, LynxReg reg) {
    return state->getFullLynxReg(reg);
}

/**
 * Function: getRegReads
 * Description: This gets all of the register reads contained within the InsInfo and places them into the 
 *  readOps array. The array is assumed to be large enough, so either pass in an array of size 32 (there 
 *  will never be this many register reads), or (info->srcOpCnt + info->readWriteOpCnt).
 * Returns: Number of register reads in info
 **/
uint8_t getRegReads(InsInfo *info, ReaderOp **readOps) {
    int pos = 0;

    int i;
    ReaderOp *curOp = info->srcOps;
    for(i = 0; i < info->srcOpCnt; i++) {
        if (curOp->type == REG_OP) {
            readOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    curOp = info->readWriteOps;
    for(i = 0; i < info->readWriteOpCnt; i++) {
        if (curOp->type == REG_OP) {
            readOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    return pos;
}

/**
 * Function: getRegWrites
 * Description: This gets all of the register writes contained within the InsInfo and places them into the 
 *  writeOps array. The array is assumed to be large enough, so either pass in an array of size 32 (there 
 *  will never be this many register writes), or (info->dstOpCnt + info->readWriteOpCnt).
 * Returns: Number of register writes in info
 **/
uint8_t getRegWrites(InsInfo *info, ReaderOp **writeOps) {
    int pos = 0;

    int i;
    ReaderOp *curOp = info->readWriteOps;
    for(i = 0; i < info->readWriteOpCnt; i++) {
        if (curOp->type == REG_OP) {
            writeOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    curOp = info->dstOps;
    for(i = 0; i < info->dstOpCnt; i++) {
        if (curOp->type == REG_OP) {
            writeOps[pos] = curOp;
            pos++;
        }
        curOp = curOp->next;
    }

    return pos; 
}

const uint8_t *nextMemRegion(ReaderState *state, uint64_t *addr, uint32_t *size) {
    return getNextMemRegion(state->memState, addr, size);
}

const char *getStrTable(ReaderState *state) {
    return state->fetchStr(state->formatState, 0);
}

uint32_t getStrTableSize(ReaderState *state) {
    return state->strTableSize(state->formatState);
}
