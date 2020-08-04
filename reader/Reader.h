#ifndef __TRACE_READER_H_
#define __TRACE_READER_H_

#define _FILE_OFFSET_BITS 64

#ifdef TARGET_WINDOWS
#define ftell _ftelli64
#define fseek _fseeki64

#else
#define ftell ftello
#define fseek fseeko

#endif

#include <stdint.h>
#include <xed-interface.h>
#include "typedefs.h"
#include "XedDisassembler.h"
#include <LynxReg.h>
#include <TraceFileHeader.h>
#include "ShadowMemory.h"
#include "ShadowRegisters.h"

#define getSelMask(x) (1 << x)

typedef enum {
    NONE_OP,
    REG_OP,
    MEM_OP,
    UNSIGNED_IMM_OP,
    SIGNED_IMM_OP
} ReaderOpType;

typedef struct {
    // Flags if the memory access generates an address or loads data
    uint8_t addrGen;
    // Segment Register used by access 
    //  NOTE: This is a LynxReg, not defined as that for space efficiency
    uint8_t seg;
    // Base Register used by access 
    //  NOTE: This is a LynxReg, not defined as that for space efficiency
    uint8_t base;
    // Index Register used by access 
    //  NOTE: This is a LynxReg, not defined as that for space efficiency
    uint8_t index;
    // Scaling used by memory access
    uint32_t scale;
    // Displacement of memory access
    uint64_t disp;
    // Address accessed by the memory operand
    uint64_t addr;
    // Size of the memory access
    uint16_t size;
} ReaderMemOp;

typedef struct ReaderOp_t {
    // Mark that is used for anything, we use it to verify operands match between trace and xed
    uint8_t mark;
    // Type of operand as defined by above enum
    ReaderOpType type;
    union {
        // Information about this in LynxReg.h
        LynxReg reg;
        ReaderMemOp mem;
        uint64_t unsignedImm;
        int64_t signedImm;
    };
    // Pointer to the next ReaderOp
    struct ReaderOp_t *next;
} ReaderOp;

typedef struct {
    // Instruction's source operands
    //  NOTE: This is not a linked list or an array USE THE ITERATOR FUNCTION
    ReaderOp srcOps[3];
    // Number of source operands
    uint8_t srcOpCnt;
    // Instruction's destination operands
    //  NOTE: This is not a linked list or an array USE THE ITERATOR FUNCTION
    ReaderOp dstOps[1];
    // Number of destination operands
    uint8_t dstOpCnt;
    // Instruction's operands that are both read from and written to
    //  NOTE: This is not a linked list or an array USE THE ITERATOR FUNCTION
    ReaderOp readWriteOps[1];
    // Number of operands written to and read from
    uint8_t readWriteOpCnt;
    // Textual representation of the instruction
    char mnemonic[128];
    // Class of the instruction as defined by XED (may be replaced)
    xed_iclass_enum_t insClass;
    uint32_t srcFlags;
    uint32_t dstFlags;
} InsInfo;

typedef struct {
    // Actual bytes of the instruction
    uint8_t binary[15];
    // Number of bytes used for instruction bytes
    uint8_t binSize;
    // Instruction's address in memory
    uint64_t addr;
    // ID of the function that executed the instruction
    uint32_t fnId;
    // ID of the source that executed the instruction
    uint32_t srcId;
    // Instruction's thread id
    uint32_t tid;
} ReaderIns;

typedef struct {
    // Type of the exception
    ExceptionType type; 
    // Linux signal or windows exception as set by EXCEPTION_TYPE
    uint8_t code;  
    // Address that caused the exception
    uint64_t addr;
    // ID of the thread that threw the exception
    uint32_t tid;  
} ReaderException;

typedef enum {
    EXCEPTION_EVENT,
    INS_EVENT
} ReaderEventType;

typedef struct {
    ReaderEventType type;        // The type of the event
    union
    {
        ReaderIns ins;    // A union containing a data struct based on the type of event
        ReaderException exception;
    };
//    uint64_t eid;        // The numeric id of the event in the trace, that is the number of the event (the first event in the trace being 0)
} ReaderEvent;

typedef struct rs {
    // The header of the trace file, see TraceFileHeader.h
    FileHeader fileHeader;
    // The trace file
    FILE *trace;
    // Flag if we are debugging
    uint8_t debug;
    // The ID of the current event
    uint64_t curId;
    // Information about the execution, see ReaderUtils.h
    ExInfo exInfo;
    // State for various things we need
    DisassemblerState *disState;
    MemState *memState;
    RegState *regState;
    FormatState formatState;
    // Function pointers to format-specific functions
    uint32_t (*readEvent) (FormatState, MemState *, RegState *, DisassemblerState *, 
        ReaderEvent *, uint64_t, InsInfo *); 
    void (*freeFormat) (FormatState); 
    const char *(*fetchStr) (FormatState, uint32_t);
    uint32_t (*findStr) (FormatState, const char *);
    uint32_t (*strTableSize) (FormatState);
    uint8_t (*hasFields) (FormatState, uint32_t);
    uint8_t (*getLynxRegSize) (LynxReg);
    LynxReg (*getFullLynxReg) (LynxReg);
} ReaderState;

//typedef struct ReaderState_t ReaderState;

ReaderState *initReader(char *filename, uint32_t debug);

void closeReader(ReaderState *state);

uint32_t nextEvent(ReaderState *state, ReaderEvent *event);

uint32_t nextEventWithCheck(ReaderState *state, ReaderEvent *nextEvent, InsInfo *curInfo);

uint32_t prevEvent(ReaderState *state, ReaderEvent *event);

uint32_t seekTo(ReaderState *state, int base);

void getMemoryVal(ReaderState *state, uint64_t addr, uint32_t size, uint8_t *buf);

const uint8_t *getRegisterVal(ReaderState *state, LynxReg reg, uint32_t thread);

const char *fetchStrFromId(ReaderState *state, uint32_t id);

uint32_t findString(ReaderState *state, const char *str);

void initInsInfo(InsInfo *info);

void fetchInsInfo(ReaderState *s, ReaderIns *ins, InsInfo *info);

void freeInsInfo(InsInfo *info);

uint64_t getCurEid(ReaderState *state);

uint32_t getNumThreads(ReaderState *state);

uint32_t loadedFromSegment(ReaderState *state, uint64_t addr);

uint8_t hasFields(ReaderState *state, uint32_t sel);

ReaderOp *nextOp(ReaderOp *curOp, uint8_t totalOpCnt, uint8_t curIt); 

uint8_t getMemReads(InsInfo *info, ReaderOp **readOps);

uint8_t getMemWrites(InsInfo *info, ReaderOp **writeOps);

uint8_t getRegReads(InsInfo *info, ReaderOp **readOps);

uint8_t getRegWrites(InsInfo *info, ReaderOp **writeOps);

ArchType getArchType(ReaderState *state);

uint8_t getAddrSize(ReaderState *state);

uint32_t getRegSize(ReaderState *state, LynxReg reg);

LynxReg getFullReg(ReaderState *state, LynxReg reg);

uint32_t numSegments(ReaderState *state);

const SegmentLoad *getSegment(ReaderState *state, int i);

const uint8_t *nextMemRegion(ReaderState *state, uint64_t *addr, uint32_t *size);

const char *getStrTable(ReaderState *state);

uint32_t getStrTableSize(ReaderState *state);

#endif // _TRD_H_
