/**
 * This file contains the instrumentation for the DataOpsTrace. Really the only important function in here
 *  is insInstrumentation which fetches instruction information and decides what analysis calls to perform
 *  based on information about the instruction and what the user specifies should be included in the trace.
 **/

#include "pin.H"
#include "PinLynxReg.h"
#include "Helpers.h"
#include <map>
#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "ShadowMemory.h"
#include "RegVector.h"
#include "DataOpsDefs.h"
#include "DataOpsTrace.h"
#include "StringTable.h"

#include <sys/stat.h> 
#include <unistd.h>
#include <fcntl.h>
//#include <gelf.h>

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

using std::map;
using std::cerr;

//default is that 
//UINT16 infoSelect = 0xfffc;
UINT16 infoSelect = 0xfffc;
bool entryPointFound = false;

bool writeSrcId = true;
bool writeFnId = true;
bool writeAddr = true;
bool writeBin = true;
bool writeSrcRegs = false;
bool writeMemReads = false;
bool writeDestRegs = true;
bool writeMemWrites = true;
bool regsWritten = true;
bool memWritten = true;

KNOB<bool> srcidOff(KNOB_MODE_WRITEONCE, "pintool", "nosrcid", "", "Don't include source IDs in the trace");
KNOB<bool> fnidOff(KNOB_MODE_WRITEONCE, "pintool", "nofnid", "", "Don't include function IDs in the trace");
KNOB<bool> addrOff(KNOB_MODE_WRITEONCE, "pintool", "noaddr", "", "Don't include the instruction's address in the trace");
KNOB<bool> binOff(KNOB_MODE_WRITEONCE, "pintool", "nobin", "", "Don't include the instruction binary in the trace");
KNOB<bool> srcRegOn(KNOB_MODE_WRITEONCE, "pintool", "srcreg", "", "Include an instruction's source registers in the trace");
KNOB<bool> memReadOn(KNOB_MODE_WRITEONCE, "pintool", "memread", "", "Include an instruction's memory reads in the trace");
KNOB<bool> destRegOff(KNOB_MODE_WRITEONCE, "pintool", "nodestreg", "", "Don't include an instruction's destination registers in the trace");
KNOB<bool> memWriteOff(KNOB_MODE_WRITEONCE, "pintool", "nomemwrite", "", "Don't include an instruction's memory writes in the trace");

/**
 * Function: pinFinish
 * Description: The function that is called when done executing
 * Output: None
 **/
void pinFinish(int, void *) {
    endFile();
}

/**
 * Function: imgInstrumentation
 * Description: The instrumentation for when an image is loaded. Currently all this does is iterate over the
 *  symbols loaded by the image, and puts the addresses and names in a map. If we execute this address, then
 *  we say we are executing that function (It seems like PIN is only giving me the symbols for functions 
 *  anyway? It's a big thing, I'd rather not get into it). Currently there is some commented out code, I'm
 *  leaving that in there because it's part of the process of trying to figure out how to get global/dynamic
 *  data. 
 * Side Effects: populates apiMap with function names
 * Outputs: None
 **/    
void imgInstrumentation(IMG img, void *v) {
    /*printf("img %s %llx %lx\n", IMG_Name(img).c_str(), (unsigned long long) IMG_LoadOffset(img), (unsigned long) IMG_SizeMapped(img));
    if(IMG_SecHead(img) == IMG_SecTail(img)) {
        printf("No sections\n");
    }
    for(SEC sec = IMG_SecHead(img); sec != IMG_SecTail(img); sec = SEC_Next(sec)) {
        printf("%llx %lx %d ", (unsigned long long) SEC_Address(sec), (unsigned long) SEC_Size(sec), SEC_Type(sec));
        if(SEC_IsReadable(sec)) {
            printf("r");
        }
        if(SEC_IsWriteable(sec)) {
            printf("w");
        }
        if(SEC_IsExecutable(sec)) {
            printf("x");
        }
        printf("\n");
    }*/
    for(SYM sym = IMG_RegsymHead(img); SYM_Valid(sym); sym = SYM_Next(sym)) {
        string undFuncName = PIN_UndecorateSymbolName(SYM_Name(sym), UNDECORATION_NAME_ONLY);
        ADDRINT symAddr = SYM_Address(sym);
        if(apiMap.find(symAddr) == apiMap.end()) {
            apiMap[symAddr] = undFuncName.c_str();
        }
    }

    if(IMG_IsMainExecutable(img)) {
        entryPoint = IMG_EntryAddress(img);
    }
}


/**
 * Function: insInstrumentation
 * Description: This is the instrumentation portion of the tracer that should only be run once per 
 *  instruction. As such it needs to do a few things:
 *   1) Determine if we are interested in the code. For example, we don't care about code that 
 *       occured before the entry
 *   2) Extract information about the instruction from PIN
 *      - Note, while we generally want to avoid dynamically allocating memory, we do it here because 
 *         hopefully this function should be called relatively few times compared to the analysis 
 *         functions, and because PIN only allows us to pass certain things to our analysis function, 
 *         such as a pointer.
 *   3) Decide what needs to be done with the information and schedule the appropriate callbacks.
 *      - This is done based on the operand information and what information the user requested be 
 *         recorded the trace
 * Output: None
 */
void insInstrumentation(INS ins, void *v) {
    const ADDRINT addr = INS_Address(ins);

    if(!entryPointFound) {
        if(addr == entryPoint) {
            PIN_RemoveInstrumentation();
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) startTrace, IARG_END);
            IMG_AddInstrumentFunction(imgInstrumentation, 0);
            PIN_AddContextChangeFunction(contextChange, 0);
            entryPointFound = true;
        }
        else {
            return;
        }
    }

    RegVector localSrcRegs;
    RegVector localDestRegs;
    RegVector *srcRegs = &localSrcRegs;
    RegVector *destRegs = &localDestRegs;

    //only dynamically allocate if we need to
    if(writeDestRegs) {
        destRegs = new RegVector();
    }
    if(writeSrcRegs) {
        srcRegs = new RegVector();
    }

    UINT32 memWriteOps = 0, memReadOps = 0;

    //UINT32 srcFlags = 0;
    UINT32 destFlags = 0;

    if(regsWritten) {
        getSrcDestInfo(ins, srcRegs, destRegs, memReadOps, memWriteOps, destFlags);
    }
    else if(memWritten) {
        //theres a more efficient way to get memory operations if they are all we care about, replace
        getSrcDestInfo(ins, srcRegs, destRegs, memReadOps, memWriteOps, destFlags);
    }

    RTN rtn = INS_Rtn(ins);
    IMG img = IMG_Invalid();

    if(RTN_Valid(rtn)) {
        SEC sec = RTN_Sec(rtn);
        if(SEC_Valid(sec)) {
            img = SEC_Img(sec);
        }
    }

    int insSize = 0;
    if(writeBin) {
        insSize = INS_Size(ins);
    }

    //record data needed to print the instruction (we record stuff like disassembly before so that if there is an exception we can still print out ins)

    //prep, use a bunch of small functions so that we can inline
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) checkPrintStatus, IARG_THREAD_ID, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) printAndReset, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR) checkInitializedStatus, IARG_THREAD_ID, IARG_END);
    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR) initThread, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) initIns, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_END);

    if(writeSrcId) {
        string srcStr;
        getSrcName(img, srcStr);
        UINT32 srcId = strTable.insert(srcStr.c_str());
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordSrcId, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_UINT32, srcId, IARG_END);
    }

    if(writeFnId) {
        string fnStr;
        getFnName(rtn, img, fnStr);
        UINT32 fnId = strTable.insert(fnStr.c_str());
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordFnId, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_UINT32, fnId, IARG_END);
    }

    if(writeAddr) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordAddr, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_INST_PTR, IARG_UINT32, insSize, IARG_END);
    }

    if(writeBin) {
        //Create buffer for instruction binary which can have a max size of 15 bytes
        UINT8 *binary = new UINT8[insSize];
        EXCEPTION_INFO *exInfo = NULL;
        PIN_FetchCode(binary, (const void *) addr, insSize, exInfo);
        //for some reason, PIN is fetching the binary for an int wrong
        if(INS_IsSyscall(ins) && INS_Opcode(ins) == XED_ICLASS_INT && binary[0] == 0xEA && binary[1] == 0x1e) {
            insSize = 2;
            binary[0] = 0xCD;
            binary[1] = 0x2E;
        }
        if(writeAddr) {
            bool *newIns = new bool(true);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordFirstBin, IARG_THREAD_ID, IARG_PTR, newIns, IARG_PTR, binary, IARG_UINT32, insSize, IARG_END);
        }
        else {
            //if address is not present, we cannot do binary prediction
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) alwaysRecordBin, IARG_THREAD_ID, IARG_PTR, binary, IARG_UINT32, insSize, IARG_END);
        }
    }

    //If we care about values, print numOps, and check memory reads (but don't necessairly write them)
    if(regsWritten || memWritten) {
        if(memReadOps == 1) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) checkMemRead, IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
        }
        else if(memReadOps == 2) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) check2MemRead, IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
        }
    }

    //inline the writing of source regs
    if(writeSrcRegs && (srcRegs->getSize() > 0)) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordSrcRegs, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_PTR, srcRegs, IARG_END); 
    }

    //here we write the mem reads
    if(writeMemReads) {
        if(memReadOps == 1) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordMemRead, IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD_SIZE, IARG_END);
        }
        else if(memReadOps == 2) {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) record2MemRead, IARG_THREAD_ID, IARG_MEMORYREAD_EA, IARG_MEMORYREAD2_EA, IARG_MEMORYREAD_SIZE, IARG_END);
        }
    }

    //if there are destination regs, print them
    if(writeDestRegs && (destRegs->getSize() > 0 || destFlags != 0)) {
        if(destRegs->getSize() == 0) {
            delete destRegs;
            destRegs = NULL;
        }

        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordDestRegs, IARG_FAST_ANALYSIS_CALL, IARG_THREAD_ID, IARG_PTR, destRegs, IARG_UINT32, destFlags, IARG_END);
    }

    if(writeMemWrites && memWriteOps == 1) {
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordMemWrite, IARG_THREAD_ID, IARG_MEMORYWRITE_EA, IARG_MEMORYWRITE_SIZE, IARG_END);
    }

    //system calls naturally don't have an AFTER, make sure everything else does
    if(!INS_IsSyscall(ins)) {
        bool printingIns = false;
        if(INS_HasFallThrough(ins)) {
            INS_InsertCall(ins, IPOINT_AFTER, (AFUNPTR) printIns, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
            printingIns = true;
        }
        //if(INS_IsDirectBranchOrCall(ins) || INS_IsRet(ins)) {
        if((INS_IsBranch(ins) || INS_IsCall(ins)) && !INS_IsXend(ins) && INS_IsValidForIpointTakenBranch(ins)) {
            INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR) printIns, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_END);
            printingIns = true;
        }
        if(!printingIns) {
            fprintf(errorFile, "not printing instruction %s\n", INS_Disassemble(ins).c_str());
        }
    }
}

/**
 * Function: checkSelections
 * Description: Checks the command line arguments and sets flags indicating what information to include in
 *  the trace.
 * Output: None
 **/
void checkSelections() {
    if(srcidOff.Value()) {
        infoSelect &= ~(1 << SEL_SRCID);
        writeSrcId = false;
    }
    if(fnidOff.Value()) {
        infoSelect &= ~(1 << SEL_FNID);
        writeFnId = false;
    }
    if(addrOff.Value()) {
        infoSelect &= ~(1 << SEL_ADDR);
        writeAddr = false;
    }
    if(binOff.Value()) {
        infoSelect &= ~(1 << SEL_BIN);
        writeBin = false;
    }
    if(srcRegOn.Value()) {
        infoSelect |= (1 << SEL_SRCREG);
        writeSrcRegs = true;
    }
    if(memReadOn.Value()) {
        infoSelect |= (1 << SEL_MEMREAD);
        writeMemReads = true;
    }
    if(destRegOff.Value()) {
        infoSelect &= ~(1 << SEL_DESTREG);
        writeDestRegs = false;
    }
    if(memWriteOff.Value()) {
        infoSelect &= ~(1 << SEL_MEMWRITE);
        writeMemWrites = false;
    }

    regsWritten = writeSrcRegs || writeDestRegs;
    memWritten = writeMemReads || writeMemWrites;
}

/**
 * Function: main
 * Description: Sets up PIN
 * Output: None actually, since you never return from this
 **/
int main(int argc, char *argv[]) {
    LEVEL_PINCLIENT::PIN_InitSymbols();

    if(PIN_Init(argc, argv)) {
        cerr << KNOB_BASE::StringKnobSummary() << endl;
        return -1;
    }

    checkSelections();

    setupFile(infoSelect);
    setupLocks();

    INS_AddInstrumentFunction(insInstrumentation, 0);
    IMG_AddInstrumentFunction(imgInstrumentation, 0);
    PIN_AddFiniFunction(pinFinish, 0);
    PIN_AddThreadStartFunction(threadStart, 0);


    // Never returns
    PIN_StartProgram();

    return 0;
}
