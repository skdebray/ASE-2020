/*
*
*
*
*/

#ifndef _CFG_STATE_H_
#define _CFG_STATE_H_
#include "cfg.h"
#include "Reader.h"
#include "Taint.h"
#include "XedDisassembler.h"

struct cfgState_t{
	struct cfg *startingPhase;
        struct cfg *curCfgStruct;
        // Global variables for IDs of blocks, functions, and egdes
        int edgeID;
        int blockID;
        int functID;
        int numThreads;
        int numInsRead;
        int isSelfMod;

        // Global variables for current block/funtion pointers, a pointer to the simulated call stack, and the pointer to the cfg
        struct function **curFunct;
        struct block **curBlock;
        struct cfgStack **stack;

        // Global hashtables 
        //struct Hashtable *htablecfg;
        //struct Hashtable *htablefunc;

        // last instruction added to cfg
        struct cfgInstruction **prevInsFromMap;
        struct cfgInstruction *pendingException;
        //ReaderState *rState;
        const char *strTable;
	uint32_t strTableSize; 
	DisassemblerState *disState;

        TaintState *taintState;
        ReaderState *readerState;
        uint64_t numB;
        uint64_t numE;
        uint64_t numP;
        uint64_t numI;
};

#endif
