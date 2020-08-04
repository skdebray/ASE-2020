/**
 * This file is intended to hold everything needed for the analysis functions of a format. In addition, it
 * includes everything that is needed to setup/finish the trace file because historically the Main file
 * contained code for multiple trace formats.
 **/

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include "PinLynxReg.h"
#include "DataOpsTrace.h"
#include "DataOpsDefs.h"
#include "Helpers.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <iostream>

using std::cout;
using std::endl;

//this will need to be adjusted if there are more than 16 threads, but this is faster than Pin's TLS
const UINT32 maxThreads = 16;

StringTable strTable;
const RegVector emptyRegVector;
UINT32 sectionOff;
UINT32 traceHeaderOff;
UINT32 traceStart;
UINT32 predTID = 0;
UINT32 numSkipped = 0;
const UINT32 LARGEST_REG_SIZE = 64;
const UINT32 BUF_SIZE = 16384;
const UINT32 UINT8_SIZE = sizeof(UINT8);
const UINT32 UINT16_SIZE = sizeof(UINT16);
const UINT32 UINT32_SIZE = sizeof(UINT32);
const UINT32 UINT64_SIZE = sizeof(UINT64);
const UINT32 ADDRINT_SIZE = sizeof(ADDRINT);

UINT8 traceBuf[BUF_SIZE];
UINT8 *traceBufPos = traceBuf;
UINT8 dataBuf[BUF_SIZE];
UINT8 *dataBufPos = dataBuf;

#if defined(TARGET_IA32)
#pragma message("x86")
#define fullLynxReg LynxReg2FullLynxIA32Reg
#define lynxRegSize LynxRegSize32
#elif defined(TARGET_IA32E)
#pragma message("x86-64")
#define fullLynxReg LynxReg2FullLynxIA32EReg
#define lynxRegSize LynxRegSize
#else
#error "Unsupported Architecture"
#endif

//thread local storage structure (for those unfamiliar with C++ structs, they are essentially classes with different privacy rules so you can have functions)
struct ThreadData {
    ThreadData() :
            eventId(0), pos(NULL), memWriteAddr(0), memWriteSize(0), destRegs(NULL), destFlags(0), print(false), initialized(false), predSrcId(0), predFuncId(0), predAddr(0), dataPos(NULL), printBinary(NULL), binary(NULL) {
    }
    UINT64 eventId;
    UINT8 buffer[38];
    UINT8 *pos;
    UINT8 *predRecord;
    ADDRINT memWriteAddr;
    UINT32 memWriteSize;
    const RegVector *destRegs;
    UINT32 destFlags;
    bool print;
    bool initialized;
    uint32_t predSrcId;
    uint32_t predFuncId;
    ADDRINT predAddr;
    UINT8 dataBuffer[512];
    UINT8 *dataPos; 
    bool *printBinary;
    uint8_t *binary;
    uint8_t binSize;
};

//Now we don't have to rely on PIN for TLS
ThreadData tls[maxThreads];

/**
 * Function: getFileBuf
 * Description: Get a buffer for the file that is guaranteed to fit the given size. Must provide the file's
 *  buffer and the current position in that buffer. 
 * Side Effects: Writes to file if there is not enough space in buffer
 * Output: the position in the buffer that it is safe to write to
 **/
UINT8 *getFileBuf(UINT32 size, UINT8 *fileBuf, UINT8 *curFilePos, FILE *file) {
    UINT16 bufSize = curFilePos - fileBuf;
    if((bufSize + size) > BUF_SIZE) {
        fwrite(fileBuf, UINT8_SIZE, bufSize, file);
        curFilePos = fileBuf;
    }

    return curFilePos;
}

/** 
 * Function: writeToFile
 * Description: The wrapper to fwrite essentially. It ensures that we write large blocks of data to a 
 *  file at once. It places information into the file buffer and, if full, flushes the buffer to the file
 *  when full.
 * Output: the new current position in the buffer.
 **/
UINT8 *writeToFile(UINT8 *buf, UINT8 *endPos, UINT8 *fileBuf, UINT8 *curFilePos, FILE *file) {
    UINT16 bufSize = curFilePos - fileBuf;
    UINT8 *curPos;
    for(curPos = buf; (curPos + UINT64_SIZE) <= endPos; curPos += UINT64_SIZE) {
        if((bufSize + UINT64_SIZE) > BUF_SIZE) {
            fwrite(fileBuf, UINT8_SIZE, bufSize, file);
            bufSize = 0;
            curFilePos = fileBuf;
        }
        *((UINT64 *)curFilePos) = *((UINT64 *) curPos);
        curFilePos += UINT64_SIZE;
        bufSize += UINT64_SIZE;
    }

    for(; curPos < endPos; curPos++) {
        if((bufSize + UINT8_SIZE) > BUF_SIZE) {
            fwrite(fileBuf, UINT8_SIZE, bufSize, file);
            bufSize = 0;
            curFilePos = fileBuf;
        }
        *curFilePos = *curPos;
        curFilePos += UINT8_SIZE;
        bufSize += UINT8_SIZE;
    }

    return curFilePos;
}

/**
 * Function: recordSrcRegs
 * Description: Records source registers and values in a thread local data buffer that will be recorded
 *  in the data section of the trace after the instruction is executed.
 * Assumptions: There is enough space in the buffer
 * Side Effects: Adds registers to data buffer
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordSrcRegs(THREADID tid, const CONTEXT *ctx, const RegVector *srcRegs) {
    ThreadData &data = tls[tid];

    UINT8 buf[LARGEST_REG_SIZE];
    for(UINT8 i = 0; i < srcRegs->getSize(); i++) {
        *((UINT8 *) data.dataPos) = (UINT8) OP_TEST_REG; // Make constants for these
        data.dataPos += UINT8_SIZE;

        LynxReg lReg = srcRegs->at(i);
        *((UINT8 *) data.dataPos) = (UINT8) lReg;
        data.dataPos += UINT8_SIZE;

        LynxReg fullLReg = fullLynxReg(lReg);
        UINT32 fullSize = lynxRegSize(fullLReg);
        REG reg = LynxReg2Reg(fullLReg);
        PIN_GetContextRegval(ctx, reg, buf);
        //pin size and Lynx size doesn't always agree
        memcpy(data.dataPos, buf, fullSize);
        data.dataPos += fullSize;
    }
}

/**
 * Function: startTrace
 * Description: Function used to mark when to start tracing.
 * Output: None
 **/
void startTrace() {
    printTrace = true;
}

/**
 * Function: printDataLabel
 * Description: Writes a data label with the given eventId to the buffer specified by pos
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printDataLabel(UINT8 *pos, UINT64 eventId) {
    *pos = (UINT8) OP_LABEL;
    pos += UINT8_SIZE;

    *((UINT64 *) pos) = eventId;
    pos += UINT64_SIZE;

    return pos;
}

/**
 * Function: printMemData
 * Description: Writes a MemData entry into the location specified by pos given the memory's 
 *  address, value and size. Additionally, if sizePos is provided, it set the sizePos's value to
 *  the location of size in the buffer.
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printMemData(UINT8 *pos, UINT8 size, ADDRINT addr, UINT8 *val, UINT8 **sizePos) {
    *pos = (UINT8) OP_MEM;
    pos += UINT8_SIZE;

    if(sizePos != NULL) {
        *sizePos = pos;
    }

    *pos = size;
    pos += UINT8_SIZE;

    *((ADDRINT *) pos) = addr;
    pos += ADDRINT_SIZE;

    if(size != 0 && val != NULL) {
        memcpy(pos, val, size);
        pos += size;
    }

    return pos;
}

/**
 * Function: printExceptionEvent
 * Description: Writes an exception event into the location specified by pos
 * Assumptions: There is enough space in the buffer
 * Output: New current position in buffer
 **/
UINT8 *printExceptionEvent(UINT8 *pos, ExceptionType eType, INT32 info, THREADID tid, ADDRINT addr) {
	UINT8 *buf = pos;

	*pos = EVENT_EXCEPT; //event_type
	pos += UINT8_SIZE;

	*pos = eType; //type
	pos += UINT8_SIZE;

	*((INT32 *) pos) = info;
	pos += sizeof(INT32);

	*((UINT32 *) pos) = tid;
	pos += UINT32_SIZE;

	*((ADDRINT *) pos) = addr;
	pos += ADDRINT_SIZE;

	pos += UINT16_SIZE;

	*((UINT16 *)(pos - UINT16_SIZE)) = (UINT16)(pos - buf);

	return pos;
}

/**
 * Function: printDataReg
 * Description: Writes a data register entry for lReg into the buffer location specified by pos.
 *  It also fills in valBuf with the value of the register
 * Assumptions: There is enough space in the buffer, valBuf is at least 64 bytes
 * Side Effects: Fills in valBuf with register value
 * Output: New current position in buffer
 **/
UINT8 *printDataReg(THREADID tid, UINT8 *pos, LynxReg lReg, const CONTEXT *ctxt, UINT8 *valBuf) {
	REG reg = LynxReg2Reg(lReg);

	PIN_GetContextRegval(ctxt, reg, valBuf);

	*pos = (UINT8) OP_REG;
	pos += UINT8_SIZE;

	*((UINT32 *) pos) = tid;
	pos += UINT32_SIZE;

	*pos = (UINT8) lReg;
	pos += UINT8_SIZE;

	UINT32 size = lynxRegSize(lReg);
	memcpy(pos, valBuf, size);
	pos += size;

	return pos;
} 

/**
 * Function: recordMemRead
 * Description: This function records information about a memory read into the thread local storage's 
 *  data buffer. This buffer gets written to the data section of the trace after the instruction is
 *  executed.
 * Output: None
 **/
void recordMemRead(THREADID tid, ADDRINT readAddr, UINT32 readSize) {
    ThreadData &data = tls[tid];

    // Insert ops_info
    *((UINT8 *)(data.dataPos)) = OP_TEST_MEM;
    data.dataPos += UINT8_SIZE;
    
    //assert(readSize <= LARGEST_REG_SIZE);
    *((UINT8 *)(data.dataPos)) = (UINT8) readSize;
    data.dataPos += UINT8_SIZE;

    *((ADDRINT *) data.dataPos) = readAddr;
    data.dataPos += ADDRINT_SIZE;

    PIN_SafeCopy(data.dataPos, (UINT8 *) (readAddr), readSize);
    data.dataPos += readSize;
}

/**
 * Function: checkMemRead
 * Description: This function needs to be executed regardless of whether we are writing memory reads 
 *  into the trace. It checks to make sure we've already seen a memory value at this address. If not,
 *  it needs to be recorded in the trace (since it won't be in the reader's shadow architecture). 
 *  It also checks to see if we are currently looking at a special memory region that gets updated by
 *  the kernel. In this case, we won't know if we saw the current value before so we also must always
 *  print out those values.
 **/
void checkMemRead(THREADID tid, ADDRINT readAddr, UINT32 readSize) {
    //ThreadData &data = tls[tid];
    PIN_MutexLock(&dataLock);
    ADDRINT curAddr = readAddr;
    UINT8 seenBits[9];
    UINT8 value[64];
    PIN_SafeCopy(value, (UINT8 *) (readAddr), readSize);

    mem.loadSeen(readAddr, readSize, seenBits);
    //UINT8 buf[4096];
    UINT8 *buf = getFileBuf(512, dataBuf, dataBufPos, dataFile);
    UINT8 *sizePos = NULL;
    UINT8 *pos = buf;

    for(UINT32 i = 0; i < readSize; i++) {
        if((seenBits[i >> 3]) & (1 << (i % 8))) {
            if(sizePos != NULL) {
                *sizePos = (pos - sizePos) - ADDRINT_SIZE - UINT8_SIZE;
                sizePos = NULL;
            }
        }
        else {
            if(sizePos == NULL) {
                pos = printMemData(pos, 0, curAddr, NULL, &sizePos);
		
            }
            
            *pos = value[i];
            pos += UINT8_SIZE;
        }
        curAddr++;
    }

    if(sizePos != NULL) {
        *sizePos = (pos - sizePos) - ADDRINT_SIZE - UINT8_SIZE;
    }

    //there was something we didn't see
    if(pos != buf) {
        mem.setSeen(readAddr, readSize);
    }
    else {
        /*
         * Used to print memory locations that we can't predict, even with the previous value. One example of this is
         * The SharedUserData structure in windows which includes a few tick count fields.
         * Source: http://uninformed.org/index.cgi?v=2&a=2&p=15
         *
         * There may be a similar structure in linux from vDSO 
         */
        for(UINT32 i = 0; i < numRegions; i++) {
            if(specialRegions[i].includes(readAddr, readSize)) {
                //places the update on the previous instruction
                //pos = printDataLabel(pos, data.eventId);
                pos = printMemData(pos, readSize, readAddr, value, NULL);
                break;
            }
        }
    }

    dataBufPos = pos;
    PIN_MutexUnlock(&dataLock);
}

/**
 * Functions: record2MemRead and check2MemRead
 * Description: Just calls the corresponding functions twice for each memory read.
 * Output: None
 **/
void record2MemRead(THREADID tid, ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize) {
    recordMemRead(tid, readAddr1, readSize);
    recordMemRead(tid, readAddr2, readSize);
}

void check2MemRead(THREADID tid, ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize) {
    checkMemRead(tid, readAddr1, readSize);
    checkMemRead(tid, readAddr2, readSize);
}

/**
 * Function: recordMemWrite
 * Description: We can only get the address of a memory write from PIN before an instruction executes.
 *  since we need this information after the instruction executes (so we can get the new values), we need
 *  to record the address and size of the memory write in thread local storage so we can get the information
 *  later.
 * Output: None
 **/
void recordMemWrite(THREADID tid, ADDRINT addr, UINT32 size) {
    //for a memory write all we need to do is record information about it so after the instruction we can print it
    ThreadData &data = tls[tid];
    data.memWriteAddr = addr;
    data.memWriteSize = size;

    PIN_MutexLock(&dataLock);
    mem.setSeen(addr, size);
    PIN_MutexUnlock(&dataLock);
}

/**
 * Function: checkPrintStatus
 * Description: Checks to see if we printed the instruction within the thread local storage for the tid.
 * Output: true if already printed, false otherwise
 **/
bool checkPrintStatus(THREADID tid) {
    return tls[tid].print;
}

/**
 * Function: printAndReset
 * Description: The only case where a function is not printed after an instruction is if a system call
 *  occurred, which is what this case handles. It prints the instruction to the trace, creates a new
 *  data label for the instruction, and records the register values. Since we don't know how memory was
 *  modified as a result of the system call, we also reset the shadow memory so that everything is marked
 *  as unseen. 
 * Output: None
 **/
void printAndReset(THREADID tid, const CONTEXT *ctx) {
    if(tls[tid].print) {
        bool labeled = printIns(tid, ctx);
        PIN_MutexLock(&dataLock);
        if(!labeled) {
            dataBufPos = getFileBuf(32, dataBuf, dataBufPos, dataFile);
            dataBufPos = printDataLabel(dataBufPos, tls[tid].eventId);
        }
        mem.reset();
        recordRegState(tid, ctx);
        PIN_MutexUnlock(&dataLock);
    }
}

/**
 * Function: checkInitializedStatus
 * Description: Checks to see if we have printed out the initial status of a thread.
 * Output: True if initialized, false otherwise
 **/
bool checkInitializedStatus(THREADID tid) {
    return !tls[tid].initialized;
}

/**
 * Function: initThread
 * Description: Records the initial state of a thread in the trace
 * Output: None
 **/
void initThread(THREADID tid, const CONTEXT *ctx) {
    if(!tls[tid].initialized) {
        PIN_MutexLock(&dataLock);
        recordRegState(tid, ctx);
        PIN_MutexUnlock(&dataLock);
        tls[tid].initialized = true;
    }
}

/**
 * Function: initIns
 * Description: Initializes the thread local storage for a new instruction. 
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL initIns(THREADID tid) {
    ThreadData &data = tls[tid];
    data.pos = data.buffer + UINT8_SIZE;
    data.dataPos = data.dataBuffer;

    //save instruction info in TLS
    *((UINT8 *) data.pos) = EVENT_INS;
    data.predRecord = data.pos;
    data.pos += UINT8_SIZE;

    //be predict everythign is right
    *data.predRecord |= 0xf8;

    //mark that this instruction needs to be printed
    data.print = true;
}

/**
 * Function: recordSrcId
 * Description: Records the source ID of the instruction in the thread local storage's trace buffer. 
 *  After the instruction executes, this buffer will be written to the trace.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordSrcId(THREADID tid, UINT32 srcId) {
    ThreadData &data = tls[tid];
    bool incorrect = (srcId != data.predSrcId);
    data.predSrcId = *((UINT32 *) data.pos) = srcId;
    *data.predRecord &= ~((incorrect) << PRED_SRCID);
    data.pos += (incorrect) ? UINT32_SIZE : 0;
}

/**
 * Function: recordFnId
 * Description: Records the function Id of the instruction into the thread local storage's trace
 *  buffer. After the instruction executes, this buffer will be written to the trace.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordFnId(THREADID tid, UINT32 fnId) { 
    ThreadData &data = tls[tid];
    bool incorrect = (fnId != data.predFuncId);
    data.predFuncId = *((UINT32 *) data.pos) = fnId; //for now assume dllId and fnId is 0
    *data.predRecord &= ~((incorrect) << PRED_FUNCID);
    data.pos += (incorrect) ? UINT32_SIZE : 0;
}

/**
 * Function: recordAddr
 * Description: Records the address of the instruction into the thread local storage's trace
 *  buffer. After the instruction executes, this buffer will be written to the trace.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordAddr(THREADID tid, ADDRINT addr, UINT32 binSize) {
    ThreadData &data = tls[tid];
    bool incorrect = addr != data.predAddr;
    *((ADDRINT *) data.pos) = addr;
    *data.predRecord &= ~((incorrect) << PRED_ADDR);
    data.pos += (incorrect) ? ADDRINT_SIZE : 0;
    data.predAddr = addr + binSize;
}

/**
 * Function: recordFirstBin
 * Description: Records the instruction's binary into the thread local storage's binary buffer. After the
 *  instruction executes, the buffer will be written to the trace. To ensure the binary only gets 
 *  printed in the trace once, it copies newIns to the tls, and only writes the binary if newIns is false
 *  when the binary is written to the trace. Then, the binary will be deleted, and delIns will be set to 
 *  false so that we don't record the binary again.
 * Output: None
 **/
void recordFirstBin(THREADID tid, bool *newIns, UINT8 *binary, UINT32 binSize) {
        ThreadData &data = tls[tid];
        data.printBinary = newIns;
        data.binary = binary;
        data.binSize = binSize;
}


/**
 * Function: alwaysRecordBin
 * Description: This function will always record the binary in the tracefile by copying it into the trace
 *  buffer. The tls's trace buffer will be written to the trace after the instruction executes.
 * Output: None
 **/
void alwaysRecordBin(THREADID tid, UINT8 *binary, UINT32 binSize) {
    ThreadData &data = tls[tid];
    memcpy(data.pos, binary, binSize);
    data.pos += binSize;
    *(data.predRecord) &= ~(PRED_CORRECT << PRED_BIN);
}

/**
 * Function: recordDestRegs
 * Description: Records the instruction's destination registers and flags in thread local storage so
 *  that they can be printed after the instruction executes.
 * Output: None
 **/
void PIN_FAST_ANALYSIS_CALL recordDestRegs(THREADID tid, const RegVector *destRegs, UINT32 destFlags) {
    ThreadData &data = tls[tid];
    data.destFlags = destFlags;
    data.destRegs = destRegs;
}

/**
 * Function: printIns
 * Description: This is the last step of the analysis for an instruction. It adds the data in the 
 *  thread local storage's buffers to destination flag, destination register and memory write data
 *  and writes everything to the appropriate file.
 * Output: None
 **/
bool printIns(THREADID tid, const CONTEXT *ctx) {
    ThreadData &data = tls[tid];

    //print instruction information and source registers

    //if flags were modified, print the new flags
    UINT8 buf[LARGEST_REG_SIZE];
    if(data.destFlags != 0) {
        *((UINT8 *) data.dataPos) = (UINT8) OP_DST_REG;
        data.dataPos += UINT8_SIZE;

        *((UINT8 *) data.dataPos) = (UINT8) LYNX_GFLAGS;
        data.dataPos += UINT8_SIZE;

        PIN_GetContextRegval(ctx, REG_GFLAGS, buf);
        UINT32 regSize = lynxRegSize(LYNX_GFLAGS);
        //pin size and Lynx size doesn't always agree
        memcpy(data.dataPos, buf, regSize);
        data.dataPos += regSize;

        data.destFlags = 0;
    }

    //print out the destination registers
    if(data.destRegs != NULL) {
        const RegVector *writeRegs = data.destRegs;
        REG reg;
        for(UINT8 i = 0; i < writeRegs->getSize(); i++) {
            *((UINT8 *) data.dataPos) = (UINT8) OP_DST_REG; // Make constants for these
            data.dataPos += UINT8_SIZE;

            LynxReg lReg = writeRegs->at(i);
            *((UINT8 *) data.dataPos) = (UINT8) lReg;
            data.dataPos += UINT8_SIZE;

            LynxReg fullLReg = fullLynxReg(lReg);
            UINT32 fullSize = lynxRegSize(fullLReg);
            reg = LynxReg2Reg(fullLReg);
            PIN_GetContextRegval(ctx, reg, buf);
            //pin size and Lynx size doesn't always agree
            memcpy(data.dataPos, buf, fullSize);
            data.dataPos += fullSize;
        }
        data.destRegs = NULL;
    }

    if(data.memWriteSize != 0) {
        // Insert ops_info
        *((UINT8 *) data.dataPos) = OP_DST_MEM;
        data.dataPos += UINT8_SIZE;
        
        *((UINT8 *) data.dataPos) = (UINT8)(data.memWriteSize);
        data.dataPos += UINT8_SIZE;

        *((ADDRINT *) data.dataPos) = data.memWriteAddr;
        data.dataPos += ADDRINT_SIZE;

        PIN_SafeCopy(data.dataPos, (UINT8 *) (data.memWriteAddr), data.memWriteSize);
        data.dataPos += data.memWriteSize;

        data.memWriteSize = 0; 
    }

    PIN_MutexLock(&traceLock);
    //printf("locked %d\n", tid);
    //assign the event id, I sometimes need the event id of the previous instruction on the same thread
    data.eventId = eventId;
    eventId++;

    if(data.printBinary != NULL && *data.printBinary) {
        *data.printBinary = false;
        memcpy(data.pos, data.binary, data.binSize);
        data.pos += data.binSize;
        delete[] data.binary;
        *(data.predRecord) &= ~(PRED_CORRECT << PRED_BIN);
    }

    if(tid != predTID) {
        *((UINT32 *) data.pos) = tid;
        data.pos += UINT32_SIZE;
        predTID = tid;
        *data.predRecord &= ~(PRED_CORRECT << PRED_TID);
    }

    bool labeled = false;

    if(data.pos == (data.buffer + 2)) {
        numSkipped++;
    }
    else {
        *((UINT8 *) data.buffer) = numSkipped;
        numSkipped = 0;
        //don't include numSkipped in count, so add uint8 not uint16
        *((UINT16 *) data.pos) = data.pos - data.buffer + UINT8_SIZE;
        data.pos += UINT16_SIZE;

        traceBufPos = writeToFile(data.buffer, data.pos, traceBuf, traceBufPos, traceFile);
    }
    if(data.dataPos != data.dataBuffer) {
        labeled = true;
        PIN_MutexLock(&dataLock);
        dataBufPos = getFileBuf(32, dataBuf, dataBufPos, dataFile);
        dataBufPos = printDataLabel(dataBufPos, data.eventId);
        dataBufPos = writeToFile(data.dataBuffer, data.dataPos, dataBuf, dataBufPos, dataFile);
        PIN_MutexUnlock(&dataLock);
    }

    //printf("unlocked %d\n", tid);

    PIN_MutexUnlock(&traceLock);

    //mark that we printed the instruction
    data.print = false;
    return labeled;
}

/**
 * Function: contextChange
 * Description: Records information in a trace if an exception occurred, resulting in a context change. 
 *  Note, this is the only place that event ids are adjusted due to an exception event.
 * Output: None
 **/
void contextChange(THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *fromCtx, CONTEXT *toCtx, INT32 info, void *v) {
    if(printTrace) {
        //check to see if an exception occurred. If so, print out info about it
        if(reason == CONTEXT_CHANGE_REASON_FATALSIGNAL || reason == CONTEXT_CHANGE_REASON_SIGNAL) {
            PIN_MutexLock(&traceLock);
            traceBufPos = getFileBuf(32, traceBuf, traceBufPos, traceFile);
            *traceBufPos = numSkipped;
            traceBufPos += 1;
            traceBufPos = printExceptionEvent(traceBufPos, LINUX_SIGNAL, info, tid, PIN_GetContextReg(fromCtx, REG_INST_PTR));
            eventId++;
            PIN_MutexUnlock(&traceLock);
        }
        else if(reason == CONTEXT_CHANGE_REASON_EXCEPTION) {
            PIN_MutexLock(&traceLock);
            traceBufPos = getFileBuf(32, traceBuf, traceBufPos, traceFile);
            *traceBufPos = numSkipped;
            traceBufPos += 1;
            traceBufPos = printExceptionEvent(traceBufPos, WINDOWS_EXCEPTION, info, tid, PIN_GetContextReg(fromCtx, REG_INST_PTR));
            eventId++;
            PIN_MutexUnlock(&traceLock);
        }
        /*Causing issues now because destRegs and destFlags are available, and apparently are not in the context
         else {
         printIns(tid, fromCtx);
         }*/
    }
}

/**
 * Function: recordRegState
 * Description: Records the register state for the current architecture in data file.
 * Output: None
 **/
void recordRegState(THREADID tid, const CONTEXT *ctxt) {
    UINT8 val[LARGEST_REG_SIZE];
    UINT8 *pos = getFileBuf(4096, dataBuf, dataBufPos, dataFile);

#if defined(TARGET_MIC) || defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_GR64_FIRST; lReg <= LYNX_GR64_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32)
    for(UINT32 lReg = LYNX_X86_GR32_FIRST; lReg <= LYNX_X86_GR32_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32) || defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_YMM_X86_FIRST; lReg <= LYNX_YMM_X86_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_IA32E)
    for(UINT32 lReg = LYNX_YMM_X64_FIRST; lReg <= LYNX_YMM_X64_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif
#if defined(TARGET_MIC)
    for(UINT32 lReg = LYNX_ZMM_FIRST; lReg <= LYNX_ZMM_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_K_MASK_FIRST; lReg <= LYNX_K_MASK_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
#endif

    for(UINT32 lReg = LYNX_SEG_FIRST; lReg <= LYNX_SEG_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_SSE_FLG_FIRST; lReg <= LYNX_SSE_FLG_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_FPU_FIRST; lReg <= LYNX_FPU_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }
    for(UINT32 lReg = LYNX_FPU_STAT_FIRST; lReg <= LYNX_FPU_STAT_LAST; lReg++) {
	    pos = printDataReg(tid, pos, (LynxReg)lReg, ctxt, val);
    }

    //for some reason if I try to print these out, PIN will crash
#if 0
    for(UINT32 lReg = LYNX_DBG_FIRST; lReg <= LYNX_DBG_LAST; lReg++) {
        printReg(tid, (LynxReg)lReg, ctxt, val, file);
    }
    for(UINT32 lReg = LYNX_CTRL_FIRST; lReg <= LYNX_CTRL_LAST; lReg++) {
        printReg(tid, (LynxReg)lReg, ctxt, val, file);
    }
#endif

    dataBufPos = pos;

    //fwrite(buf, UINT8_SIZE, pos - buf, dataFile);

}

/**
 * Function: threadStart
 * Description: Call this when another thread starts so we can accurately track the number of threads 
 *  the program has. We also need to initialize the thread local storage for the new thread. Note, we 
 *  check here to make sure we are still within maxThreads
 * Output: None
 **/
VOID threadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, void *v) {
    if(tid >= maxThreads) {
        fprintf(stderr, "More than %u threads are being used", maxThreads);
        exit(1);
    }
    //we've got another thread
    numThreads++;

    ThreadData &data = tls[tid];
    data.initialized = false;
    data.pos = data.buffer;
    data.dataPos = data.dataBuffer;
    data.predRecord = NULL;
    data.print = false;
    data.destRegs = NULL;
    data.destFlags = 0;
    data.memWriteSize = 0;
}

/**
 * Function: setupFile
 * Description: Opens and sets up any output files. Note, since this is an ASCII trace, it will setup 
 *  trace.out according to the Data Ops trace file format.
 * Output: None
 */
void setupFile(UINT16 infoSelect) {
    traceFile = fopen("trace.out", "wb");
    dataFile = fopen("data.out", "w+b");
    errorFile = fopen("errors.out", "w");

    FileHeader h;
    h.ident[0] = 'U';
    h.ident[1] = 'A';
    h.ident[2] = 'T';
    h.ident[3] = 'R';
    h.ident[4] = 'C';
    h.ident[5] = 2;
    h.ident[6] = 0;
    h.ident[7] = 0;
    h.ident[8] = 0;

    h.traceType = DATA_OPS_TRACE;

#if defined(TARGET_IA32)
    h.archType = X86_ARCH;
#else
    h.archType = X86_64_ARCH;
#endif

    int x = 1;
    char *c = (char *) &x;

    //we only support little-endian right now.
    assert(c);

    h.machineFlags = *c;
    h.sectionNumEntry = 5;

    sectionOff = sizeof(FileHeader);
    UINT32 sectionSize = sizeof(SectionEntry);
    traceHeaderOff = sectionOff + h.sectionNumEntry * sectionSize;
    UINT64 infoSelHeaderOff = traceHeaderOff + sizeof(TraceHeader);
    traceStart = infoSelHeaderOff + sizeof(InfoSelHeader);

    h.sectionTableOff = sectionOff;

    h.sectionEntrySize = sectionSize;


    fwrite(&h, sizeof(FileHeader), 1, traceFile);

    SectionEntry entries[h.sectionNumEntry];

    strncpy((char *) entries[0].name, "TRACE", 15);
    entries[0].offset = traceStart;
    entries[0].type = TRACE_SECTION;

    strncpy((char *) entries[1].name, "DATA", 15);
    entries[1].type = DATA_SECTION;

    strncpy((char *) entries[2].name, "TRACE_HEADER", 15);
    entries[2].type = TRACE_HEADER_SECTION;
    entries[2].offset = traceHeaderOff;
    entries[2].size = sizeof(TraceHeader);

    strncpy((char *) entries[3].name, "STR_TABLE", 15);
    entries[3].type = STR_TABLE;

    /*strncpy((char *) entries[4].name, "SEGMENT_LOADS", 15);
    entries[4].type = SEGMENT_LOADS;*/

    strncpy((char *) entries[4].name, "INFO_SEL_HEAD", 15);
    entries[4].type = INFO_SEL_HEADER;
    entries[4].offset = infoSelHeaderOff;
    entries[4].size = sizeof(InfoSelHeader);

    //entries[5].offest = 

    fwrite(entries, sectionSize, h.sectionNumEntry, traceFile);

    TraceHeader traceHeader;
    fwrite(&traceHeader, sizeof(TraceHeader), 1, traceFile);

    InfoSelHeader infoSelHeader;
    infoSelHeader.selections = infoSelect;
    fwrite(&infoSelHeader, sizeof(InfoSelHeader), 1, traceFile);
}

/**
 * Function: endFile
 * Description: The function that will complete writing the trace file. To do so, it needs to:
 *    1) Calculate and store the size of the trace section and the size and location of the data section
 *    2) Store the number of threads that the application uses in the trace
 *    3) Copy the data from the data file into a section in the trace
 * Output: None
 **/
void endFile() {
    fwrite(traceBuf, 1, traceBufPos - traceBuf, traceFile);
    fwrite(&numSkipped, 1, 1, traceFile);
    UINT64 tracePos = ftell(traceFile);

    UINT64 traceSize = tracePos - traceStart;
    UINT64 dataSize = ftell(dataFile) + dataBufPos - dataBuf;

    const int BUFSIZE = 4096;
    UINT8 buf[BUFSIZE];

    //copy datafile into the end of the trace
    rewind(dataFile);

    int amtRead;
    while((amtRead = fread(buf, 1, BUFSIZE, dataFile))) {
        fwrite(buf, amtRead, 1, traceFile);
    }

    fwrite(dataBuf, 1, dataBufPos - dataBuf, traceFile);

    strTable.dumpTable(traceFile);
    UINT64 strPos = tracePos + dataSize;
    UINT64 strSize = strTable.getTotalStrSize();

    /*fwrite(segments, sizeof(SegmentLoad), numSegments, traceFile);
    UINT64 segmentPos = strPos + strSize;
    UINT64 segmentSize = sizeof(SegmentLoad) * numSegments;*/

    fseek(traceFile, sectionOff + sizeof(SectionEntry) - UINT64_SIZE, SEEK_SET);
    fwrite(&traceSize, sizeof(traceSize), 1, traceFile);
    fseek(traceFile, sectionOff + 2 * sizeof(SectionEntry) - 2 * UINT64_SIZE, SEEK_SET);
    fwrite(&tracePos, sizeof(tracePos), 1, traceFile);
    fwrite(&dataSize, sizeof(dataSize), 1, traceFile);
    fseek(traceFile, sectionOff + 4 * sizeof(SectionEntry) - 2 * UINT64_SIZE, SEEK_SET);
    fwrite(&strPos, sizeof(strPos), 1, traceFile);
    fwrite(&strSize, sizeof(strSize), 1, traceFile);
    /*fseek(traceFile, sectionOff + 5 * sizeof(SectionEntry) - 2 * UINT64_SIZE, SEEK_SET);
    fwrite(&segmentPos, sizeof(segmentPos), 1, traceFile);
    fwrite(&segmentSize, sizeof(segmentSize), 1, traceFile);*/
    fseek(traceFile, traceHeaderOff, SEEK_SET);
    fwrite(&numThreads, UINT32_SIZE, 1, traceFile);

    fclose(dataFile);
    fclose(traceFile);
    fclose(errorFile);
}
