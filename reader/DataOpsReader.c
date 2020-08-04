/**
 * This file contains all of the code that is required exclusively to parse the DataOps trace format. 
 *  Anything that may be useful for another trace format reader I have tried to extract into ReaderUtils.c.
 *  Thus, this file almost exclusively consists of the code necessary to initialize/fetch/free the state 
 *  required to read the trace format and read data from the trace.
 **/

#include <assert.h>
#include <DataOpsDefs.h>
#include "Buf.h"
#include <XedDisassembler.h>
#include "ShadowMemory.h"
#include "ShadowRegisters.h"
#include "Verify.h"
#include "DataOpsReader.h"

typedef struct PredictionRecord_t {
    // prediction for the next source ID
    uint32_t predSrcId;
    // prediction for the next function ID
    uint32_t predFnId;
    // prediction for the next instruction address
    uint64_t predAddr;
} PredictionRecord;

struct DataOpsState_t {    
    // trace's string table
    char *strTable;
    uint32_t tableSize;
    // structure defined by the DataSelect enum in TraceFileHeader.h that holds fields used by trace
    uint16_t selections;
    // size of an instruction's address, either 4 or 8 bytes
    uint8_t addrSize;
    // The predicted thread ID (Only need one of these per trace)
    uint32_t predTID;
    // The thread specific predictions
    PredictionRecord *predictions;
    // The number of instructions to generate from predictions, when 0 read from trace
    uint32_t numSkips;
    // The buffer for the trace section of the file
    Buf *traceBuf;
    // The current position within the traceBuf
    uint8_t *tracePos;
    // The buffer for the data section of the file
    Buf *dataBuf;
    // The current position within the dataBuf
    uint8_t *dataPos;
    // The instruction ID of the next time we need to read from the data section
    uint64_t nextDataId;
    // Function pointer to parse an instruction's address so we don't have to constantly check addr size
    uint64_t (*parseAddr)(uint8_t *);
    // Function pointer that will get the appropriate size of a register for 32 and 64 bit architectures
    uint8_t (*archLynxRegSize)(LynxReg);
    // Function pointer that will get the "full register" (largest one that contains the requested one)
    //  for 32 and 64 bit architectures
    LynxReg (*LynxRegFull) (LynxReg reg);
};

uint32_t dataOpsStrTableSize(DataOpsState *state) {
    return state->tableSize;
}

/**
 * Function: parseDataOpsData
 * Description: Parses the next data section in the trace. It should be noted, that it does not check to see
 *  if the id of the next label matches the current id, so only call this function when the two match. It
 *  takes in the optional InsInfo parameter. If provided, it will check the state of the data it reads in
 *  to see if it agrees with our data. Either way, however, it reads in data and updates the Shadow Arch
 *  appropriately.
 * Side Effects: The Shadow Architecture is updated with values from data and nextDataId is updated with
 *  the event id of the next label in the data section. If info is provided, the data will be checked which
 *  automatically outputs warnings to the user.
 * Outputs: None
 **/
void parseDataOpsData(DataOpsState *state, MemState *memState, RegState *regState, InsInfo *info) {
    //try to get the first byte, if we can't then we are at the end of the data section
    Buf *dataBuf = state->dataBuf;
    uint8_t addrSize = state->addrSize;
    uint8_t (*archLynxRegSize)(LynxReg) = state->archLynxRegSize;
    uint64_t (*parseAddr)(uint8_t *) = state->parseAddr;
    LynxReg (*LynxRegFull) (LynxReg reg) = state->LynxRegFull;
    uint32_t curTID = state->predTID;

    uint8_t *dataPos = loadN(dataBuf, state->dataPos, 1);
    
    if (dataPos == NULL) {
        return;
    }

    initializeMarks(info);

    uint16_t size;
    uint8_t opType;

    //get buf
    while(*dataPos != OP_LABEL) {
        switch(opType = *dataPos) {
            case OP_REG: {
                dataPos += 1;
                dataPos = loadN(dataBuf, dataPos, 5);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                uint32_t tid = *((uint32_t *) dataPos);
                dataPos += 4;

                LynxReg lReg = (LynxReg) *dataPos;
                dataPos += 1;

                size = archLynxRegSize(lReg);

                dataPos = loadN(dataBuf, dataPos, size);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                setThreadVal(regState, lReg, tid, dataPos);
                dataPos += size;
                break;
            } 
            case OP_DST_MEM: 
            case OP_MEM: {
                dataPos += 1;
                dataPos = loadN(dataBuf, dataPos, addrSize + 2);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                size = *((uint16_t *) dataPos);
                dataPos += 2;

                uint64_t memAddr = parseAddr(dataPos);
                dataPos += addrSize;

                dataPos = loadN(dataBuf, dataPos, size);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                store(memState, memAddr, size, dataPos);
                dataPos += size;

                if (info != NULL && opType == OP_DST_MEM) {
                    findDstMem(memAddr, size, info);
                }
                break;
            } 
            case OP_DST_REG: {
                dataPos += 1;
                dataPos = loadN(dataBuf, dataPos, 5);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                LynxReg dstReg = (LynxReg) *dataPos;
                LynxReg fullReg = LynxRegFull(dstReg);
                dataPos += 1;

                size = archLynxRegSize(fullReg);

                dataPos = loadN(dataBuf, dataPos, size);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                setThreadVal(regState, fullReg, curTID, dataPos);
                dataPos += size;

                if (info != NULL) {
                    findDstReg(dstReg, info);
                }
                break;
            } 
            case OP_TEST_REG: {
                dataPos += 1;
                dataPos = loadN(dataBuf, dataPos, 5);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                LynxReg testReg = (LynxReg) *dataPos;
                LynxReg fullReg = LynxRegFull(testReg);
                dataPos += 1;

                size = archLynxRegSize(fullReg);

                dataPos = loadN(dataBuf, dataPos, size);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                if (info != NULL) {
                    findSrcReg(testReg, info);

                    const uint8_t *regVal = getThreadVal(regState, fullReg, curTID);

                    char msg[256];
                    sprintf(msg, "Register value %s mismatch: %s", LynxReg2Str(fullReg), info->mnemonic);

                    checkVals(dataPos, regVal, size, msg);
                }

                dataPos += size;
                break;
            } 
            case OP_TEST_MEM: {
                dataPos += 1;
                dataPos = loadN(dataBuf, dataPos, addrSize + 2);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }

                size = *((uint16_t *) dataPos);
                dataPos += 2;

                uint64_t memAddr = parseAddr(dataPos);
                dataPos += addrSize;

                dataPos = loadN(dataBuf, dataPos, size);
                if (dataPos == NULL) {
                    throwError("Data Section ended in the middle of data");
                }
 
                if (info != NULL) {
                    findSrcMem(memAddr, size, info);

                    uint8_t memVal[MAX_MEM_OP_SIZE];
                    load(memState, memAddr, size, memVal);

                    char msg[256];

                    sprintf(msg, "Memory value at %llx mismatch: %s", 
                        (unsigned long long) memAddr, info->mnemonic);

                    checkVals(dataPos, memVal, size, msg);
                }

                dataPos += size;
                break;
            } 
            default:
                dataPos += 1;
                throwError("Invalid data operand type found");
        }

        //try to get the next label, if we can't that's fine because we are at the beginning of new data
        dataPos = loadN(dataBuf, dataPos, 1);
        if (dataPos == NULL) {
            state->dataPos = NULL;
            return;
        }
    }

    dataPos += 1;

    //we got a label, but no event id
    dataPos = loadN(dataBuf, dataPos, 8);
    if (dataPos == NULL) {
        throwError("Data Section ended in the middle of data");
    } 

    state->nextDataId = *((uint64_t *) dataPos);
    state->dataPos = dataPos + 8;

    if (info != NULL) {
        checkMarks(info, state->selections);
    }
}

/**
 * Function: initDataOps
 * Description: Initializes the reader for the DataOps format. This requires setting up any architecture
 *  specific requirements, reading any format-specific headers, and initializing any state. One important
 *  thing to note is that this will call the data parsing function so that the initial state gets set.
 * Side Effects: Quite a bit gets initialized
 * Output: None
 **/
DataOpsState *initDataOps(FileHeader *fileHeader, SectionEntry * sectionTable, FILE *traceFile, ExInfo *exInfo) {
    uint16_t i;

    DataOpsState *state = malloc(sizeof(DataOpsState));
    if (state == NULL) {
        throwError("couldn't allocate DataOpsState");
    }
    state->strTable = NULL;
    state->selections = 0;
    state->numSkips = 0;

    TraceHeader traceHeader;
    InfoSelHeader infoSelHeader;

    uint8_t foundTrace = 0;
    uint8_t foundData = 0;
    uint8_t foundStrTable = 0;
    uint8_t foundInfoSelHeader = 0;
    uint8_t foundTraceHeader = 0;
    //uint8_t foundSegmentLoads = 0;

    switch(fileHeader->archType) {
        case X86_ARCH:
            state->parseAddr = parseAddr32;
            state->addrSize = 4;
            state->archLynxRegSize = LynxRegSize32;
            state->LynxRegFull = LynxReg2FullLynxIA32Reg;
            break;
        case X86_64_ARCH:
            state->parseAddr = parseAddr64;
            state->addrSize = 8;
            state->archLynxRegSize = LynxRegSize;
            state->LynxRegFull = LynxReg2FullLynxIA32EReg;
            break;
        default:
            throwError("Unknown Architecture Type");
    }

    SegmentLoad *segments = NULL;
    int numSegments = 0;

    for(i = 0; i < fileHeader->sectionNumEntry; i++) {
        switch(sectionTable[i].type) {
            case TRACE_SECTION: //this section was read in Trd.c apparently
                foundTrace = 1;
                state->traceBuf = readTraceEntry(traceFile, sectionTable + i);
                state->tracePos = state->traceBuf->buf + 1;

                if (state->traceBuf == NULL) {
                    throwError("Unable to Read Trace Section");
                }
                break;
            case DATA_SECTION: // this section was read in Trd.c apparently
                foundData = 1;
                state->dataBuf = readDataEntry(traceFile, sectionTable + i);
                state->dataPos = state->dataBuf->buf;

                if (state->dataBuf == NULL) {
                    throwError("Unable to Read Data Section");
                }
                break;
            case TRACE_HEADER_SECTION:
                foundTraceHeader = 1;

                if (readTraceHeaderEntry(traceFile, sectionTable + i, &traceHeader) != 1) {
                    throwError("Unable to Read Trace Header");
                }
                break;
            case STR_TABLE:
                foundStrTable = 1;
                state->strTable = readStrTableEntry(traceFile, sectionTable + i);
                state->tableSize = sectionTable[i].size;

                if (state->strTable == NULL) {
                    throwError("Unable to Read String Table");
                }
                break;
            case INFO_SEL_HEADER:
                foundInfoSelHeader = 1;

                if (readInfoSelHeaderEntry(traceFile, sectionTable + i, &infoSelHeader) != 1) {
                    throwError("Unable to Read Info Sel Header");
                }

                state->selections = infoSelHeader.selections;
                break;
            case SEGMENT_LOADS:
                //foundSegmentLoads = 1;

                numSegments = readSegmentLoads(traceFile, sectionTable + i, &segments);

                if (numSegments < 0) {
                    throwError("Unable to read Segment Loads");
                }
                break;
        }
    }

    if (!foundTrace) {
        free(state);
        throwError("Unable to Locate Trace Section in File");
    }
    if (!foundData) {
        free(state);
        throwError("Unable to Locate Data Section in File");
    }
    if (!foundTraceHeader) {
        free(state);
        throwError("Unable to Locate Trace Header in File");
    }
    if (!foundStrTable) {
        free(state);
        throwError("Unable to Locate String Table in File");
    }
    if (!foundInfoSelHeader) {
        free(state);
        throwError("Unable to Locate Information Selection Header in File");
    }
    /*if (!foundSegmentLoads) {
        free(state);
        throwError("Unable to Locate Segment Loads Section in File");
    }*/

    state->predictions = malloc(sizeof(PredictionRecord) * traceHeader.numThreads);

    state->predTID = 0;
    state->nextDataId = 0;
    for(i = 0; i < traceHeader.numThreads; i++) {
        state->predictions[i].predSrcId = 0;
        state->predictions[i].predFnId = 0;
        state->predictions[i].predAddr = 0;
    }

    if (exInfo != NULL) {
        exInfo->numThreads = traceHeader.numThreads;
        exInfo->loadSegments = segments;
        exInfo->numSegments = numSegments;
    }
    else {
        free(segments);
    }

    return state;
}

void setupDataOpsInitState(DataOpsState *state, MemState *memState, RegState *regState) {
    parseDataOpsData(state, memState, regState, NULL);
}

/**
 * Function: freeDataOps
 * Description: Frees state used by the DataOps format. One thing to note is that it currently does not
 *  free the string table so that if instructions are being kept around, the user can still get the name
 *  of libraries and functions used by the instruction.
 * Output: None
 **/
void freeDataOps(DataOpsState *state) {
    freeBuf(state->traceBuf);
    freeBuf(state->dataBuf);
    free(state->predictions);
    free(state->strTable);
    free(state);
}

/**
 * Function: genInsFromPred
 * Description: Generates an instruction from predicted values. Note that the predicted value for binary is
 *  the value stored at that address in shadow memory.
 * Side Effects: The input ReaderIns is populated
 * Output: None
 **/
static void genInsFromPred(DataOpsState *state, MemState *memState, RegState *regState, DisassemblerState *disState, ReaderIns *ins) {
    PredictionRecord *predictions = state->predictions;

    ins->binSize = 0;
    ins->tid = state->predTID;

    ins->srcId = predictions[ins->tid].predSrcId;

    ins->fnId = predictions[ins->tid].predFnId;

    ins->addr = predictions[ins->tid].predAddr;

    if (state->selections & (1 << SEL_BIN)) {
        xed_decoded_inst_t xedIns;
        load(memState, ins->addr, 15, ins->binary);

        ins->binSize = decodeIns(disState, &xedIns, ins->binary, 15);

        uint64_t predAddr = ins->addr + ins->binSize;
        predictions[ins->tid].predAddr = predAddr;
        setThreadVal(regState, LYNX_RIP, ins->tid, (uint8_t *)(&predAddr));
    }
}

/**
 * Function: parseIns
 * Description: Parses an instruction from the trace. Currently it has to save the position of quite a few
 *  data elements because in many cases the TID is required, which is among the last items to be parsed.
 *  Note that it will read the instruction form pos.
 * Side Effects: The input ReaderIns gets populated. The predictions are updated.
 * Output: The position in the buffer immediately after the instruction
 **/
static uint8_t *parseIns(DataOpsState *state, MemState *memState, RegState *regState, DisassemblerState *disState, ReaderIns *ins, uint8_t *pos) {
    PredictionRecord *predictions = state->predictions;
    
    uint8_t *startingPos = pos;
    uint32_t *srcIdPos = NULL;
    uint32_t *fnIdPos = NULL;
    uint8_t *addrPos = NULL;

    ins->binSize = 0;

    if ((*pos & 0x7) != EVENT_INS) {
        throwError("Exception found was not an instruction");
    }
    uint8_t predResults = *pos & (~0x7);
    pos += 1;

    //get source
    if (!((predResults >> PRED_SRCID) & PRED_CORRECT)) {
        srcIdPos = (uint32_t *) pos;
        pos += 4;
    }

    if (!((predResults >> PRED_FUNCID) & PRED_CORRECT)) {
        fnIdPos = (uint32_t *) pos;
        pos += 4;
    }

    if (!((predResults >> PRED_ADDR) & PRED_CORRECT)) {
        addrPos = pos;
        pos += state->addrSize;
    }

    if (!((predResults >> PRED_BIN) & PRED_CORRECT)) {
        xed_decoded_inst_t xedIns;
        ins->binSize = decodeIns(disState, &xedIns, pos, 15);
        
        memcpy(ins->binary, pos, ins->binSize);

        if (ins->binSize == 0) {
            throwError("Unable to parse binary from trace");
        }

        pos += ins->binSize;
    }

    if ((predResults >> PRED_TID) & PRED_CORRECT) {
        ins->tid = state->predTID;
    }
    else {
        ins->tid = *((uint32_t *) pos);
        pos += 4;
        state->predTID = ins->tid;
    }


    if (srcIdPos == NULL) {
        ins->srcId = predictions[ins->tid].predSrcId;
    }
    else {
        ins->srcId = *srcIdPos;
        predictions[ins->tid].predSrcId = *srcIdPos;
    }

    if ((predResults >> PRED_FUNCID) & PRED_CORRECT) {
        ins->fnId = predictions[ins->tid].predFnId;
    }
    else {
        ins->fnId = *fnIdPos;
        predictions[ins->tid].predFnId = *fnIdPos;
    }

    if ((predResults >> PRED_ADDR) & PRED_CORRECT) {
        ins->addr = predictions[ins->tid].predAddr;
    }
    else {
        ins->addr = state->parseAddr(addrPos);
    }

    if (ins->binSize != 0) {
        store(memState, ins->addr, ins->binSize, ins->binary);
    }
    else if (state->selections & (1 << SEL_BIN)) {
        xed_decoded_inst_t xedIns;
        load(memState, ins->addr, 15, ins->binary);

        ins->binSize = decodeIns(disState, &xedIns, ins->binary, 15);
        if (ins->binSize == 0) {
            throwError("Unable to read instruction binary from shadow memory");
        }
    }

    predictions[ins->tid].predAddr = ins->addr + ins->binSize;
    setThreadVal(regState, LYNX_RIP, ins->tid, (uint8_t *)(&predictions[ins->tid].predAddr));

    if ((pos - startingPos + 2) != *((uint16_t *) pos)) {
        throwError("Event size does not match expected size");
    }

    pos += 2;

    return pos;
}


/**
 * Function: parseException
 * Description: Parses an exception from the input buffer and populates exception.
 * Side Effects: The ReaderException is populated
 * Output: The position in the buffer just after the exception
 **/
static uint8_t *parseException(DataOpsState *state, ReaderException *exception, uint8_t *buf) {
    uint8_t *startingPos = buf;
    if (*buf != EVENT_EXCEPT) {
        throwError("Event found was not an exception");
    }
    buf += 1;

    exception->type = *((uint8_t *) buf);
    buf += 1;

    if (exception->type != LINUX_SIGNAL && exception->type != WINDOWS_EXCEPTION) {
        throwError("Unknown Exception Type");
    }

    exception->code = *((uint32_t *) buf);
    buf += 4;

    exception->tid = *((uint32_t *) buf);
    buf += 4;

    exception->addr = state->parseAddr(buf);
    buf += state->addrSize;

    if ((buf - startingPos + 2) != *((uint16_t *) buf)) {
        throwError("Event size does not matcch expected event size");
    }

    buf += 2;

    return buf;
}

/**
 * Function: readDataOpsEvent
 * Description: Performs all of the steps necessary to read the next instruction with the id curId. If
 *  the id of the next instruction matches the next label in the data section, data will be read first.
 *  Note, this is the only thing curInfo is needed for, and is allowed to be NULL since it is optional to
 *  provide curInfo to parseDataOpsData. After reading the data, it will read the next event or instruction
 *  from the trace and populate nextEvent.
 * Side Effects: nextEvent is populated
 * Output: 1 if an instruction was parsed, 0 otherwise
 **/
uint32_t readDataOpsEvent(DataOpsState *state, MemState *memState, RegState *regState, DisassemblerState *disState, ReaderEvent *nextEvent, uint64_t curId, InsInfo *curInfo) {
    Buf *traceBuf = state->traceBuf;
    uint8_t *tracePos = state->tracePos;

    if ((curId - 1) == state->nextDataId) {
        parseDataOpsData(state, memState, regState, curInfo);
    }

    if (state->numSkips == 0) {
        uint8_t *tmpPos;
        if ((tmpPos = loadN(traceBuf, tracePos, 38)) == NULL) {
            uint64_t remaining = getBytesRemaining(traceBuf, tracePos);
            if (remaining == 0) {
                return 0;
            }
            
            tmpPos = loadN(traceBuf, tracePos, remaining);
        }

        tracePos = tmpPos;

        switch(*(tracePos) & 0x7) {
            case EVENT_INS:
                nextEvent->type = INS_EVENT;
                tracePos = parseIns(state, memState, regState, disState, &nextEvent->ins, tracePos);
                break;
            case EVENT_EXCEPT:
                nextEvent->type = EXCEPTION_EVENT;
                tracePos = parseException(state, &nextEvent->exception, tracePos);
                break;
            default:
                throwError("Unknown Event Type");
        }

        state->numSkips = *tracePos;
    }
    else {
        nextEvent->type = INS_EVENT;
        genInsFromPred(state, memState, regState, disState, &nextEvent->ins);
        state->numSkips--;
    }

    //only pass numSkips byte once we have actually finished doing the correct number of skips. If
    //we don't, the reader could mark that we have finished the trace prematurely for traces with
    //skips at the end
    if (state->numSkips == 0) {
        tracePos = loadN(traceBuf, tracePos, 1);
        tracePos += 1;
    }

    state->tracePos = tracePos;

    return 1;
}

/**
 * Function: fetchDataOpsStr
 * Description: Returns the string with the given id from the string table.
 * Output: The sting with the matching id.
 **/
const char *fetchDataOpsStr(DataOpsState *state, uint32_t id) {
    return state->strTable + id;
}

uint8_t hasDataOpsField(DataOpsState *state, uint32_t query) {
    return ((query & state->selections) == query);
}

uint32_t findDataOpsStr(DataOpsState *state, const char *str) {
    const char *end = state->strTable + state->tableSize;

    const char *curStr;
    for(curStr = state->strTable; curStr != end; curStr += strlen(curStr) + 1) {
        if(strcmp(curStr, str) == 0) {
            return curStr - state->strTable;
        }
    }

    return -1;
} 
