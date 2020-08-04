#include "cfgAPI.h"
#include "cfg.h"
#include "cfgState.h"
#include "utils.h"
#include "XedDisassembler.h"
#include "TraceFileHeader.h"
#include "hashtable.h"
#include "function.h"
#include "edge.h"
#include <inttypes.h>

Bbl *getBlock(uint64_t addr, cfgState *cfgState){
  cfgInstruction *cfgIns = get_key_val(cfgState->curCfgStruct->htablecfg, addr);
  if(cfgIns != NULL) {
    return cfgIns->block;
  }
  return NULL;
}

DisassemblerState *getDisState(cfgState *cfgStructArg){
  return cfgStructArg->disState;
}

void fetchInsInfoCFGState(cfgState *cfgStructArg, cfgInstruction *cfgIns, cfgInsInfo *info){
  fetchInsInfoNoReaderState(cfgStructArg->disState, cfgIns, info);
}

Function *getFunction(Bbl *block){
  return(block->fun);
}

// Traversal starting functions
Function *getStartingFunct(cfg *cfg){
  return(cfg->startingpoint);
}
 
Bbl *getStartingBlock(Function * funct){
  return(funct->first);
}

cfgInstruction *getStartingIns(Bbl *block){
  return(block->first);
}

Edge *getStartingSuccessor(Bbl *block){
  return(block->succs);
}

Edge *getStartingPredecessor(Bbl *block){
  return(block->preds);
}

// Traversal functions
Function *getNextFunction(Function *curFunct){
  return(curFunct->next);
}

Bbl *getNextBlock(Bbl *curBlock){
  return(curBlock->next);
}

cfgInstruction *getNextInstruction(cfgInstruction *curIns){
  return(curIns->next);
}

Edge *getNextEdge(Edge *curEdge){
  return(curEdge->next);
}

cfgInstruction *getCfgInstruction(cfgState *state, uint64_t addr) {
    return get_key_val(state->curCfgStruct->htablefunc, addr);
}

void clearInsertionPoint(cfgState *state, uint32_t tid) {
    state->prevInsFromMap[tid] = NULL;

    Function *startingFunctReal = newFunctionWithPseudoEntry(NULL, state);
    insertIntoFunctionList(state->curCfgStruct, startingFunctReal);
    put_key_val(state->curCfgStruct->htablefunc, startingFunctReal->id, (void *)startingFunctReal);
    //state->curCfgStruct->last = startingFunctReal;
    state->curFunct[0] = startingFunctReal;
    state->curBlock[0] = startingFunctReal->entry;
    return;
}

void markedTainted(cfgInstruction *ins){
  ins->tainted = 1;
}

void setCfgInsertionPoint(cfgState *state, uint32_t tid, uint64_t addr) {
    cfgInstruction *cfgIns = get_key_val(state->curCfgStruct->htablecfg, addr);
    if(cfgIns != NULL) {
        state->prevInsFromMap[tid] = cfgIns;
        state->curBlock[tid] = cfgIns->block;
        state->curFunct[tid] = cfgIns->block->fun;
    }
    else {
        printf("Couldn't find %llx\n", (unsigned long long) addr);
    }
    return;
}

void setInsertionPoint(cfgInstruction *prevInsFromMap, Function **curFunct, Bbl **curBlock, hashtable *htablecfg, cfgState *cfgStructArg){
  cfgStructArg->prevInsFromMap[0] = prevInsFromMap;
  cfgStructArg->curFunct = curFunct;
  cfgStructArg->curBlock = curBlock;
  cfgStructArg->curCfgStruct->htablecfg = htablecfg;
}

// Duplicating functions
cfgInstruction *copyCFGInstruction(cfgInstruction *cop){
  cfgInstruction *new = alloc(sizeof(cfgInstruction));
  new->next = cop->next;
  new->prev = cop->prev;
  new->block = cop->block;
  memcpy(&(cop->event), &(new->event), sizeof(ReaderEvent));
  //new.event = alloc(sizeof(ReaderEvent));
  if (cop->event.type == INS_EVENT){
    new->event.ins.binSize = cop->event.ins.binSize;
    new->event.ins.addr = cop->event.ins.addr;
    new->event.ins.fnId = cop->event.ins.fnId;
    new->event.ins.srcId = cop->event.ins.srcId;
    new->event.ins.tid = cop->event.ins.tid;
    // copy the bytes array
    //new->event.ins.binary = malloc(15*sizeof(uint8_t));
    int walk;
    for(walk = 0; walk < 15; walk++){
      new->event.ins.binary[walk] = cop->event.ins.binary[walk];
    }
  } else { // Exception type
    //new->event.exception = malloc(sizeof(ReaderException));
    new->event.exception.type = cop->event.exception.type;
    new->event.exception.code = cop->event.exception.code;
    new->event.exception.addr = cop->event.exception.addr;
    new->event.exception.tid = cop->event.exception.tid;
  }
  return(new);
}

Edge *copyEdge(Edge *cop){
  Edge *new = alloc(sizeof(Edge));
  new->id = cop->id;
  new->from = cop->from;
  new->to = cop->to;
  new->count = cop->count;
  new->flags = cop->flags;
  new->prev = cop->prev;
  new->next = cop->prev;
  new->auxinfo = cop->auxinfo;
  return(new);
}

Edge *copyEdgeList(Bbl *new, Bbl *cop, Edge *list){
    // Copy edges successors
  if(list != NULL){
    Edge *newList = NULL;
    newList = copyEdge(list);
    newList->from = new;
    Edge *walk = newList;
    Edge *temp;
    for(temp = list->next; temp != NULL; temp = getNextEdge(temp)){
      Edge *ahead = copyEdge(temp);
      walk->next = ahead;
      ahead->prev = walk;
      walk = ahead;
    }
    return(newList);
  }
  return(NULL);
}

Bbl *copyBlock(Bbl *cop){
  Bbl *new = alloc(sizeof(Bbl));
  new->id = cop->id;
  new->btype = cop->btype;

  // Copy instructructions
  if(cop->first != NULL){ // If it is not a pseudo-block
    new->first = copyCFGInstruction(cop->first);
    new->first->block = new;
    new->last = copyCFGInstruction(cop->last);
    new->last->block = new;
    cfgInstruction *walk = new->first;
    cfgInstruction *temp;
    for(temp = cop->first->next; temp != cop->last; temp = getNextInstruction(temp)){
      cfgInstruction *ahead = copyCFGInstruction(temp);
      ahead->block = new;
      walk->next = ahead;
      ahead->prev = walk;
      walk = ahead;
    }
    walk->next = new->last;
    new->last->prev = walk;
  }

  // Copy edges successors
  if(cop->succs != NULL){
   new->succs = copyEdgeList(new, cop, cop->succs); 
  }
  // Copy edges predecessors
  if(cop->preds != NULL){
   new->preds = copyEdgeList(new, cop, cop->preds);
  }

  new->fun = cop->fun;
  new->count = cop->count;
  new->flags = cop->flags;
  new->next = cop->next;
  new->prev = cop->prev;
  new->auxinfo = cop->auxinfo;
  return(new);
}

const char *getStrTableCFG(cfgState *state){
  return(state->strTable);
}

uint32_t getStrTableSizeCFG(cfgState *state){
  return(state->strTableSize);
}

ArchType getXedArchTypeFromCFG(cfgState *state){
  return(getXedArchType(state->disState));
}

void getSharedCount(hashtable *one, hashtable *two){
  return(union_cnt(one, two));
}

int get_size(hashtable *hasht){
  return(hasht->size);
}

int isPseudoBlock(Bbl* block){
	return (block->btype == BT_UNKNOWN || block->btype == BT_ENTRY || block->btype == BT_EXIT);
}

uint64_t getReaderEventAddr(cfgInstruction* ins){
    if(ins == NULL){
        printf("ERROR: Can't get address of null instruction");
        return -1;
    }

    ReaderEvent event = ins->event;
    if(event.type == INS_EVENT) {
        printf("%llx\n", (long long int)event.ins.addr);
        return event.ins.addr;
    }
    if(event.type == EXCEPTION_EVENT){
        printf("%llx\n", (long long int)event.ins.addr);
        return event.exception.addr;
    }
    printf("ERROR: detected an invalid event type.\n");
    return 0;
}

//fetch the thread id from an instruction
int getThreadID(cfgInstruction* ins){
	if(ins == NULL){
		return -1;
	}

	if(ins->event.type == EXCEPTION_EVENT)
		return ins->event.exception.tid;
	else if(ins->event.type==INS_EVENT)
		return ins->event.ins.tid;
	else
		return 0;
}

// Tests if given function is in a phase
int isInPhase(Function *func, int phaseID){
  return(funPartOfPhase(func, phaseID));
}

// Tests if given edge is in a phase
int edgIsInPhase(Edge *edg, int phaseID){
  return(edgPartOfPhase(edg, phaseID));
}
