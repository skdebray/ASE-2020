#include "cfg.h"
#include "hashtable.h"
#include <inttypes.h>

Bbl *getBlock(uint64_t addr, cfgState *cfgState);

Function *getFunction(Bbl *block);

DisassemblerState *getDisState(cfgState *cfgStructArg);

void fetchInsInfoCFGState(cfgState *cfgStructArg, cfgInstruction *cfgIns, cfgInsInfo *info);

void cloneBlock();

void cloneFunction();

int blockIsReachable(Bbl *source, Bbl *target);

int functIsReachable(Function *source, Function *target);

// Traversal starting functions
Function *getStartingFunct(cfg *cfg);

Bbl *getStartingBlock(Function * funct);

cfgInstruction *getStartingIns(Bbl *block);

Edge *getStartingSuccessor(Bbl *block);

Edge *getStartingPredecessor(Bbl *block);

cfgInstruction *getCfgInstruction(cfgState *state, uint64_t addr);

// Traversal functions
Function *getNextFunction(Function *curFunct);

Bbl *getNextBlock(Bbl *curBlock);

cfgInstruction *getNextInstruction(cfgInstruction *curIns);

Edge *getNextEdge(Edge *curEdge);

void markedTainted(cfgInstruction *ins);

// Insertion function
void clearInsertionPoint(cfgState *state, uint32_t tid);

void setCfgInsertionPoint(cfgState *state, uint32_t tid, uint64_t addr);

void setInsertionPoint(cfgInstruction *prevInsFromMap, Function **curFunct, Bbl **curBlock, hashtable *htablecfg, cfgState *cfgStructArg);

// Duplicating functions
cfgInstruction *copyCFGInstruction(cfgInstruction *cop);

Edge *copyEdge(Edge *cop);

Bbl *copyBlock(Bbl *cop);

// String table stuff
const char *getStrTableCFG(cfgState *state);

uint32_t getStrTableSizeCFG(cfgState *state);

ArchType getXedArchTypeFromCFG(cfgState *state);

void getSharedCount(hashtable *one, hashtable *two);

int get_size(hashtable *hasht);
int isPseudoBlock(Bbl* block);

uint64_t getReaderEventAddr(cfgInstruction* ins);

int getThreadID(cfgInstruction* ins);

int isInPhase(Function *func, int phaseID);

int edgIsInPhase(Edge *edg, int phaseID);
