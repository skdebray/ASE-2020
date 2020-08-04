#ifndef _INFO_SEL_TRACE_H_
#define _INFO_SEL_TRACE_H_

#include "pin.H"
#include "TraceFileHeader.h"
#include "RegVector.h"
#include "Trace.h"
#include "StringTable.h"

//records information in TLS
void record2MemRead(THREADID tid, ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize);
void recordMemRead(THREADID tid, ADDRINT addr, UINT32 size);
void recordMemWrite(THREADID tid, ADDRINT addr, UINT32 size);
void checkMemRead(THREADID tid, ADDRINT readAddr, UINT32 readSize);
void check2MemRead(THREADID tid, ADDRINT readAddr1, ADDRINT readAddr2, UINT32 readSize);

bool checkPrintStatus(THREADID tid);
void printAndReset(THREADID tid, const CONTEXT *ctx);
bool checkInitializedStatus(THREADID tid);
void initThread(THREADID tid, const CONTEXT *ctx);
void PIN_FAST_ANALYSIS_CALL recordSrcRegs(THREADID tid, const CONTEXT *ctx, const RegVector *srcRegs);
void PIN_FAST_ANALYSIS_CALL initIns(THREADID tid);
void PIN_FAST_ANALYSIS_CALL recordSrcId(THREADID tid, UINT32 srcId);
void PIN_FAST_ANALYSIS_CALL recordFnId(THREADID tid, UINT32 fnId);
void PIN_FAST_ANALYSIS_CALL recordAddr(THREADID tid, ADDRINT addr, UINT32 binSize);
void recordFirstBin(THREADID tid, bool *newIns, UINT8 *binary, UINT32 binSize);
void alwaysRecordBin(THREADID tid, UINT8 *binary, UINT32 binSize);
void PIN_FAST_ANALYSIS_CALL recordDestRegs(THREADID tid, const RegVector *destRegs, UINT32 destFlags);

//prints information
bool printIns(THREADID tid, const CONTEXT *ctx);
void recordRegState(THREADID tid, const CONTEXT *ctxt);

//handles architectural events
void contextChange(THREADID tid, CONTEXT_CHANGE_REASON reason, const CONTEXT *fromCtx, CONTEXT *toCtx, INT32 info, void *v);
void startTrace();
VOID threadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v);

//handling the trace format
void setupFile(UINT16 infoSelect);
void endFile();

extern StringTable strTable;

#endif
