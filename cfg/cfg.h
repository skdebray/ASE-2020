/*
* File: cfg.h
* Author: Jesse Bartels
* Description: Collection of structs required to make a cfg.
*
*/

#ifndef _CFG_H_
#define _CFG_H_
#include "Reader.h"
#include <XedDisassembler.h>

// Macro to see if an instruction follows another instruction
#define StraightLineCode(ins1, ins2) ((ins1.addr + ins1.binSize) == ins2.addr)

// Parent struct for entire cfg, just holds pointer to the starting
// function. May be more helpful once we need to have multiple cfg's
typedef struct cfg {
	int id;
	struct function *startingpoint;
        struct function *last;
        // Hashtables associated with each cfg
        struct Hashtable *htablecfg;
        struct Hashtable *htablefunc;
	struct cfg *nextPhase;
	struct cfg *prevPhase;
} cfg;

typedef struct cfgState_t cfgState;

// Struct for instructions (uses reader's version of instructions, plus 
// a reference to the block the instruction resides in and a linked list
typedef struct cfgInstruction{
	ReaderEvent event;
	struct cfgInstruction *next, *prev; // linked list of instructions
	struct block *block; // Block the instruction resides in
	uint8_t fetched;
	ReaderOpType type;
	uint8_t tainted;
	union {
	  uint64_t unsignedImm;
	  int64_t signedImm;
	};
	uint8_t keep;
        uint8_t dynamic;
        struct cfgInstruction *prevVersion;
        int phaseID;
} cfgInstruction; 

// Enum for Basic Block Types
typedef enum bbl_type_ {
	BT_NORMAL, // No explicit control Transfer, execution falls through
	BT_CALL,   // block ends in function call
	BT_COND,   // block ends in a conditional branch
	BT_UNCOND, // block ends in unconditional branch
	BT_INDIR,  // block ends in an indirect jump
	BT_SWITCH, // switch statement
	BT_RET,    // return from a function has (ret mnemonic)
	BT_UNKNOWN,// psuedo block for unknowns (ex jumps with unknown targets)
	BT_ENTRY,  // pseudo block for entry into function
	BT_EXIT    // pseudo block for end of function
} bbl_type;

// struct for our basic blocks
typedef struct block {
	int id;                          // unique id
	bbl_type btype;		 // see above for possible block type
        cfgInstruction *first, *last;    // doubly linked list of instructions
	struct edge *preds, *succs;      // predecessor and succcessor blocks
	struct function *fun;		 // function block belongs to
	long count;			 // execution count
	unsigned long flags;		 
	struct block *prev, *next;	 // previous/next block in list of blocks for function
	void *auxinfo;			 // any extra info
	uint8_t visited;	//used for traversing the cfg
        struct edge *fallThroughIn, *fallThroughOut;
        struct cfg *cfgPhase;
        struct phaseList *phases;
} Bbl;

// enum for edge types
typedef enum edge_type_ {
	ET_NORMAL,   // fall-though edge from a block that does not end in jmp
	ET_TRUE,     // edges corresponing to a conditional jmp (true)
	ET_FALSE,    // edges corresponding to a conidional jmp (false)
	ET_JMP,      // direct unconditional jump
	ET_INDIR,    // indirect jump
	ET_CALL,     // call edge
	ET_RET,      // return edge
	ET_ENTRY,    // pseudo-edges for entry/exit of a function
	ET_EXIT,     // see ET_ENTRY
	ET_LINK,     // call-return link pseudo-edge
	ET_EXCEPTION,// control transfer due to exception
        ET_DYNAMIC,  // edge to connect cfgs created through dynamically modified code
        ET_GHOST
} edge_type;

// struct to hold edge information
typedef struct edge {
	int  id; 		// unique id
	edge_type etype;   // see edge_type enum for details
	struct block *from, *to;// from basic block 'x' -> to basic block 'y'
	long count;		// execution count
	unsigned long flags;	// flags
	struct edge *prev,*next;// next edge/previous edge
        // Dynamic fields for dynamic edge
        uint8_t isDynamic;     
        struct edge *dynamicSource,*dynamicTarget;
	void *auxinfo;		// auxillary info
	uint8_t visited;	//used for traversing cfg
        struct cfg *cfgPhase;
        struct phaseList *phases;
        uint8_t alreadyPrintedInAnotherPhase;
} Edge;

// struct to hold the tuple produced by addnewEdgeWithType
typedef struct edgeTuple {
	struct edge *pred, *succ;
	
} edgeTuple;

// struct for function information
typedef struct function {
	int id;			      // unique id
	const char *name;		      // name of function
	struct block *first, *last;   // first/last block in funtion
	struct block *entry, *exit;   // entry/exit block into function
	long count;		      // execution count
	unsigned long flags;	      // flags
	struct function *prev, *next; // next/previous function
	void *auxinfo;		      // auxillary info
        struct cfg *cfgPhase;
        struct phaseList *phases;
} Function;

//struct for cfgStack

typedef struct cfgStack {
	uint64_t addr;
	struct block *block;
	struct cfgStack *next;
} cfgStack;

// InsInfo for CFG use
typedef struct cfgInsInfo {
  char mnemonic[128];
  xed_iclass_enum_t insClass;
} cfgInsInfo;

typedef struct phaseList {
        int phase;
        struct phaseList *next;
} phaseList;


// Methods for the CFG Tool

void printCollisions(cfgState *cfgStructArg);

// Function Name: initCFG(ReaderState rState, cfg *cfgToPopulate)
// Purpose: Initialize the cfg struct so that instructions can be fed into the cfg struct
// Exceptions/Errors: If alloc fails then an error is thrown
// Arguments: ReaderState to build the cfg with (from initReader) and number of threads in trace
cfgState *initCFG(ReaderState *rState, int numThreads);
cfgState *initCFG2(DisassemblerState *disState, char *strTable, int size, int numthreads);

// Function Name: addInstructionToCFG(ReaderEvent *currEvent)
// Purpose: Function to add one instruction at a time to the cfg
// Exceptions/Errors: If alloc fails or if dynamically modified code is found then an error is thrown
//                    If dynamically modified code is found then an error is thrown
// Arguments: Reader event to add to the cfg
cfgInstruction *addInstructionToCFG(ReaderEvent *currEvent, cfgState *cfgStructArg);

// Function Name: closeCFG()
// Purpose: Closes the cfg struct that was passed into initCFG 
// Exceptions/Errors: If alloc fails then an error is thrown
// Arguments: None
void finalizeCFG(cfgState *cfgStructArg);

// Function Name: compareInstructions(cfgInstruction * prevIns, cfgInstruction *currIns)
// Purpose: Function to compare the instruction bytes of two different instructions
// Exceptions/Errors: None
// Arguments: The two instructions to compare
int compareInstructions(cfgInstruction *arg, cfgInstruction *arg2);

// Function Name: createDotFile(char *dotFileName, cfg *cfgArg)
// Purpose: Construct a dotfile given a control flow graph
// Exceptions/Errors: None
// Arguments: Pointer to a cfg struct to create the dot file off of
//void createDotFile(char *dotFileName, cfg *cfgArg);

cfg *getStartCFG(cfgState *state);

void mergeConditionals(cfgState *cfgStructArg);

void printNumBlocks(cfgState *cfgStructArg);

void printNumEdges(cfgState *cfgStructArg);

void printNumPhases(cfgState *cfgStructArg);

void printNumIns(cfgState *cfgStructArg);

#endif // _CFG_H_
