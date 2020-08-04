/*
* File: cfg.c
* Author: Jesse Bartels
* Description: The core cfg functions
*
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cfgState.h"
#include "cfg.h"
#include <Reader.h>
#include <Taint.h>
#include "utils.h"
#include "hashtable.h"
#include "controlTransfer.h"
#include "dot.h"
#include "block.h"
#include "edge.h"
#include "function.h"

// Function Name: compareInstructions
// Purpose: Compare two instructions to see if the are the same (looks at the instruction bytes)
// Returns: 1(true) if the instructions are the same, 0 if not
// Exceptions/Errors from library calls: None
int compareInstructions(cfgInstruction *arg, cfgInstruction *arg2){
  if (arg->event.ins.addr != arg2->event.ins.addr || arg->event.ins.binSize != arg2->event.ins.binSize){
    return 0;
  }
  return (memcmp(arg->event.ins.binary, arg2->event.ins.binary, arg->event.ins.binSize) == 0);
}

// Function Name: isDynamicallyModifiedCode(cfgInstruction *currIns, hashtable *htablecfg)
// Purpose: Compare the given instruction with what is already in the hashtable. If the addresses 
//          match but the instruction bytes differ then there is dynamically modified code
// Returns: 1(true) if the instructions are the same, 0 if not
// Exceptions/Errors from library calls: None
int isDynamicallyModifiedCode(cfgInstruction *currIns, cfgState *cfgStructArg){
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  int alreadySavedPrevVersion = 0; 
  // First check if we need to update the version of the cfgIns
  if (get_key_val(htablecfg, currIns->event.ins.addr) != NULL 
     && !(compareInstructions(get_key_val(htablecfg, currIns->event.ins.addr),currIns))){
    // Save the old instruction, update current bytes
    currIns->prevVersion = get_key_val(htablecfg, currIns->event.ins.addr);
    alreadySavedPrevVersion = 1;
  }
  // Then check if we have a new phase
  uint64_t label = 0;
  label = getMemTaintLabel(cfgStructArg->taintState, currIns->event.ins.addr, currIns->event.ins.binSize, label);
  if(label != 0){
    //printf("Label for dynamic: %ld\n", label);
    if(alreadySavedPrevVersion == 0){ // Note: if bytes don't change still save previous version, i.e. different phase
      currIns->prevVersion = get_key_val(htablecfg, currIns->event.ins.addr);
    }
    return 1;
  }
  return 0;
}

// Function Name: updateOrCreateEdge
// Purpose: Look to increment the count of an edge if it already exists or create an edge
//          if no edge exists.
// Returns: Nothing
// Exceptions/Errors from library calls: Alloc can fail
void updateOrCreateEdge(Edge *isThereAnEdge, cfgInstruction *prevInsFromMap, cfgInstruction *currInsFromMap, cfgState *cfgStructArg){
  if (isThereAnEdge == NULL){
    createEdge(prevInsFromMap, currInsFromMap, cfgStructArg);
  } else {
    if(isThereAnEdge->cfgPhase != cfgStructArg->curCfgStruct){
      addToEdgePhaseList(isThereAnEdge, cfgStructArg->curCfgStruct->id);
      isThereAnEdge->cfgPhase = cfgStructArg->curCfgStruct;
    }
    isThereAnEdge->count++;
  }
}

cfgState *initCFG2(DisassemblerState *disState, char *strTable, int size, int numthreads){
  cfgState *cfgStructArg = malloc(sizeof(cfgState));
  cfgStructArg->curCfgStruct = malloc(sizeof(cfg));
  cfgStructArg->strTable = strTable;
  cfgStructArg->strTableSize = size;
  cfgStructArg->numThreads = numthreads;
  cfgStructArg->curCfgStruct->htablecfg = create_table();
  cfgStructArg->curCfgStruct->htablefunc = create_table();
  cfgStructArg->pendingException = NULL;
  cfgStructArg->disState = disState;

  // use a simple counter for block/function id's. Start at 0 since the pseudo block starts at zero. These values are global
  cfgStructArg->blockID = 0;
  cfgStructArg->functID = 0;
  cfgStructArg->edgeID = 0;
  cfgStructArg->numInsRead = 0;
  cfgStructArg->isSelfMod = 0;

  // Allocate space for each thread's pointer(s)
  cfgStructArg->prevInsFromMap = alloc(numthreads*sizeof(cfgInstruction *));
  cfgStructArg->stack = alloc(numthreads*sizeof(cfgStack *));
  cfgStructArg->curFunct = alloc(numthreads*sizeof(Function *));
  cfgStructArg->curBlock = alloc(numthreads*sizeof(Bbl *));
  int inti;
  for(inti = 0; inti < numthreads; inti++){
    cfgStructArg->prevInsFromMap[inti] = NULL;
    cfgStructArg->stack[inti] = NULL;
    cfgStructArg->curFunct[inti] = NULL;
    cfgStructArg->curBlock[inti] = NULL;
  }

  Function *startingFunctReal = newFunctionWithPseudoEntry(NULL, cfgStructArg);
  put_key_val(cfgStructArg->curCfgStruct->htablefunc, 0, (void *)startingFunctReal);
  cfgStructArg->curCfgStruct->startingpoint = startingFunctReal;
  cfgStructArg->curCfgStruct->last = startingFunctReal;
  cfgStructArg->curFunct[0] = startingFunctReal;
  cfgStructArg->curBlock[0] = startingFunctReal->entry;
  return(cfgStructArg);
}

// Function Name: initCFG(cfg *cfgStructArg))
// Purpose: Initialize the state necessary for the cfg tool to build a control flow graph
// Returns: Nothing
// Exceptions/Errors from library calls: If alloc fails then this function will fail as well
cfgState *initCFG(ReaderState *rState, int numthreads){
  cfgState *cfgStructArg = malloc(sizeof(cfgState));
  cfg *startingPhase = malloc(sizeof(cfg));
  startingPhase->nextPhase = NULL;
  startingPhase->prevPhase = NULL;
  cfgStructArg->curCfgStruct = startingPhase;
  cfgStructArg->curCfgStruct->id = 0;
  cfgStructArg->startingPhase = startingPhase;
  cfgStructArg->strTable = getStrTable(rState);
  cfgStructArg->strTableSize = getStrTableSize(rState);
  cfgStructArg->numThreads = numthreads;
  cfgStructArg->curCfgStruct->htablecfg = create_table();
  cfgStructArg->curCfgStruct->htablefunc = create_table();
  cfgStructArg->pendingException = NULL;
  cfgStructArg->curCfgStruct->id = 1;
  cfgStructArg->numB = 0;
  cfgStructArg->numE = 0;
  cfgStructArg->numP = 1;

  // Set up xed disassembler
  //perform architecture-dependent setup
  switch (getArchType(rState)) {
    case X86_ARCH:
      cfgStructArg->disState = setupXed_x86();
      break;
    case X86_64_ARCH:
      cfgStructArg->disState = setupXed_x64();
      break;
    default:
      printf("Unknown trace architecture\n");
      exit(1);
  }

  // use a simple counter for block/function id's. Start at 0 since the pseudo block starts at zero. These values are global
  cfgStructArg->blockID = 0;
  cfgStructArg->functID = 0;
  cfgStructArg->edgeID = 0;
  cfgStructArg->numInsRead = 0;
  cfgStructArg->isSelfMod = 0;

  // Allocate space for each thread's pointer(s)
  cfgStructArg->prevInsFromMap = alloc(numthreads*sizeof(cfgInstruction *));
  cfgStructArg->stack = alloc(numthreads*sizeof(cfgStack *));
  cfgStructArg->curFunct = alloc(numthreads*sizeof(Function *));
  cfgStructArg->curBlock = alloc(numthreads*sizeof(Bbl *));
  int inti;
  for(inti = 0; inti < numthreads; inti++){
    cfgStructArg->prevInsFromMap[inti] = NULL;
    cfgStructArg->stack[inti] = NULL;
    cfgStructArg->curFunct[inti] = NULL;
    cfgStructArg->curBlock[inti] = NULL;
  }

  Function *startingFunctReal = newFunctionWithPseudoEntry(NULL, cfgStructArg);
  put_key_val(cfgStructArg->curCfgStruct->htablefunc, 0, (void *)startingFunctReal);
  cfgStructArg->curCfgStruct->startingpoint = startingFunctReal;
  cfgStructArg->curCfgStruct->last = startingFunctReal;
  cfgStructArg->curFunct[0] = startingFunctReal;
  cfgStructArg->curBlock[0] = startingFunctReal->entry;
  cfgStructArg->taintState = initTaint(rState);
  cfgStructArg->readerState = rState;
  return(cfgStructArg);
}

// Function Name: addFirstInstruction(cfgInstruction *currIns)
// Purpose: Adding the first instruction into a cfg entails creating the first starting block and edge from the pseudoblock
//          as well as initializng our linked list of blocks.
// Returns: Void
// Exceptions/Errors from library calls: If alloc fails then this function will as well
void addFirstInstruction(cfgInstruction *currIns, cfgState *cfgStructArg){
  // Pull out saved state of cfg
  cfgInstruction **prevInsFromMap = cfgStructArg->prevInsFromMap;
  Bbl **curBlock = cfgStructArg->curBlock;
  Function **curFunct = cfgStructArg->curFunct;
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  hashtable *htablefunc = cfgStructArg->curCfgStruct->htablefunc;

  cfgStructArg->numInsRead++;   
  // Create a non-pseudo starting block with current instruction being the first instruction in our cfg
  Bbl *startingBlockReal = newBlock(NULL, currIns, cfgStructArg);
  addInsIntoBlock(startingBlockReal, currIns);
  addBlockIntoFunction(curFunct[0], startingBlockReal);
  curFunct[0]->name = cfgStructArg->strTable+(currIns->event.ins.fnId); // Set the starting function's name
  curFunct[0]->id = currIns->event.ins.fnId;
  put_key_val(htablefunc, curFunct[0]->id, (void *)curFunct[0]); 
  addNewEdgeWithType(ET_ENTRY, curBlock[0], startingBlockReal, cfgStructArg);
    
  // Our current block is now the starting block
  curBlock[0] = startingBlockReal;
  put_key_val(htablecfg, currIns->event.ins.addr, (void *)currIns);
  prevInsFromMap[0] = get_key_val(htablecfg, currIns->event.ins.addr);
}

// Function name: setupThread()
// Purpose: Setup globals/new functions if we find a new thread
// Exceptions: If alloc fails due to a lack of memory
// Arguments: Current instruction being added into cfg
void setupCfgForNewThread(cfgInstruction *curIns, cfgState *cfgStructArg){
  // Pull our data from the saved state stored in the cfgStructArg
 // printf("Got to new thread setup\n");
  cfgInstruction **prevInsFromMap = cfgStructArg->prevInsFromMap;
  Bbl **curBlock = cfgStructArg->curBlock;
  Function **curFunct = cfgStructArg->curFunct;
  hashtable *htablefunc = cfgStructArg->curCfgStruct->htablefunc;

  prevInsFromMap[curIns->event.ins.tid] = curIns;
  Bbl *blockInNewThread = newBlock(NULL, curIns, cfgStructArg);
  addInsIntoBlock(blockInNewThread, curIns);
  curBlock[curIns->event.ins.tid] = blockInNewThread;
  // Determine whether we have seen this function before
  if(curIns->block->fun != NULL) {
    curFunct[curIns->event.ins.tid] = get_key_val(htablefunc, curIns->block->fun->id);
  }

  // If we have not seen this function before then we need to make a new function
  if (curFunct[curIns->event.ins.tid] == NULL){
    Function *newFunct = newFunctionWithPseudoEntry(curIns, cfgStructArg);
    insertIntoFunctionList(cfgStructArg->curCfgStruct, newFunct);
    curFunct[curIns->event.ins.tid] = newFunct;
    put_key_val(htablefunc, newFunct->id, (void *)newFunct);
  }

  addBlockIntoFunction(curFunct[curIns->event.ins.tid], blockInNewThread);
  addNewEdgeWithType(ET_ENTRY, curFunct[curIns->event.ins.tid]->first, curIns->block, cfgStructArg);
}

// Function Name: handleAppearanceOfNewThread(cfgInstruction *currIns)
// Purpose: If we see a new thread we have two cases to handle:
//            New thread with old instruction: See if we need to split block and update pointers
//            New thread with new instruction: See if we need to create a new function and add an entry block
// Returns: Nothing
// Exceptions/Errors from library calls: If alloc fails then this function will fail as well
void handleAppearanceOfNewThread(cfgInstruction *currIns, cfgState *cfgStructArg){
  // Pull data out of cfg state struct
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  Function **curFunct = cfgStructArg->curFunct;
  Bbl **curBlock = cfgStructArg->curBlock;
  cfgInstruction **prevInsFromMap = cfgStructArg->prevInsFromMap;

  // Determine if we have seen this instruction before 
  cfgInstruction *instructionLocatedInDifferentThread = get_key_val(htablecfg, currIns->event.ins.addr);
  if (instructionLocatedInDifferentThread != NULL){
    cfgInstruction *currInsFromMap = instructionLocatedInDifferentThread;
    currInsFromMap->event.ins.tid = currIns->event.ins.tid;
    curFunct[currInsFromMap->event.ins.tid] = currInsFromMap->block->fun;
    curBlock[currInsFromMap->event.ins.tid] = currInsFromMap->block;    
 
    // If we have seen this instruction before but it is in a different thread, ask whether we are about to split a block
    if (compareInstructions(currInsFromMap, currInsFromMap->block->first) == 0){
      splitBlock(NULL, currInsFromMap, NULL, currInsFromMap->block, cfgStructArg);
    }

    addNewEdgeWithType(ET_ENTRY, currInsFromMap->block->fun->first, currInsFromMap->block, cfgStructArg); 
    prevInsFromMap[currIns->event.ins.tid] = currInsFromMap;
    currInsFromMap = NULL;

  } else if (cfgStructArg->numInsRead == 0){
    addFirstInstruction(currIns, cfgStructArg);
  } else {
    put_key_val(htablecfg, currIns->event.ins.addr, (void *)currIns);
    cfgInstruction *inserted = get_key_val(htablecfg, currIns->event.ins.addr);
   // printf("Calling setup for new thread\n");
    setupCfgForNewThread(inserted, cfgStructArg);
  }
}

// Function Name: setCfgForException(cfgInstruction *currIns, cfgState *cfgStructArg)
// Purpose: Setup the CFG when we get an exception
// Returns: Nothing
// Exceptions/Errors from library calls: If alloc fails
void setCfgForException(cfgInstruction *currIns, cfgState *cfgStructArg){
  // Get our saved state
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  hashtable *htablefunc = cfgStructArg->curCfgStruct->htablefunc;
  cfgInstruction ** prevInsFromMap = cfgStructArg->prevInsFromMap;
  Function **curFunct = cfgStructArg->curFunct;
  Bbl **curBlock = cfgStructArg->curBlock;
  int tid = currIns->event.ins.tid;

  // Check to see whether we've seen the exception handling code before
  cfgInstruction *exceptionHandlingCode = get_key_val(htablecfg, currIns->event.ins.addr);
  if (exceptionHandlingCode != NULL){
    Bbl *entryPseudoIntoExceptionHandlingCode = exceptionHandlingCode->block->fun->entry;
    Edge *edgeBetween = searchThroughSuccessors(prevInsFromMap[tid]->block, entryPseudoIntoExceptionHandlingCode);
    // See if an edge already exists
    if (edgeBetween != NULL){
     // If the exception type matches
       int exceptionCodeInEdge = ((cfgInstruction *)(edgeBetween->auxinfo))->event.exception.code;
       if (exceptionCodeInEdge == cfgStructArg->pendingException->event.exception.code){
         return; // We have already seen this exception before from the previous basic block
       } 
    }
  } else {
    // If not then we need to put the exception handling code into the hashmap
    put_key_val(htablecfg, currIns->event.ins.addr, (void *)currIns);
    cfgInstruction *exceptionHandlingCode = get_key_val(htablecfg, currIns->event.ins.addr);

    // Create a new function for the exception 
    Function *functForException = newFunctionWithPseudoEntry(exceptionHandlingCode, cfgStructArg);
    pushOntoFunctionStack(prevInsFromMap[tid], exceptionHandlingCode, cfgStructArg);
    put_key_val(htablefunc, functForException->id, (void *)functForException);
    insertIntoFunctionList(cfgStructArg->curCfgStruct, functForException);

    // Insert exception handing code into a new block and connect it to the pseudo-block
    Bbl *newBlockForException = newBlock(prevInsFromMap[tid], exceptionHandlingCode, cfgStructArg);
    addInsIntoBlock(newBlockForException, exceptionHandlingCode);
    addBlockIntoFunction(functForException, newBlockForException);
    addNewEdgeWithType(ET_ENTRY, functForException->entry, newBlockForException, cfgStructArg);
  }

  // Get the exception handling code
  exceptionHandlingCode = get_key_val(htablecfg, currIns->event.ins.addr);  
  // create edge and link (also determine prevBlock type)
  Bbl *entryPseudoIntoExceptionHandlingCode = exceptionHandlingCode->block->fun->entry;
  edgeTuple *predNSucc = addNewEdgeWithType(ET_EXCEPTION, prevInsFromMap[tid]->block, entryPseudoIntoExceptionHandlingCode, cfgStructArg);
  determineBlockTypeFromEdgeType(ET_EXCEPTION, prevInsFromMap[tid]->block);

  // Get the successor/predecessor to store meta-data
  Edge *succ = predNSucc->succ;
  Edge *pred = predNSucc->pred;
  cfgInstruction *exceptionInfo = alloc(sizeof(cfgInstruction));
  exceptionInfo = memcpy(exceptionInfo, cfgStructArg->pendingException, sizeof(cfgInstruction));
  succ->auxinfo = (void *) exceptionInfo;
  pred->auxinfo = (void *) exceptionInfo;
  cfgStructArg->pendingException = NULL;

  // Update pointers
  curFunct[tid] = exceptionHandlingCode->block->fun;
  curBlock[tid] = exceptionHandlingCode->block;
  cfgStructArg->prevInsFromMap[tid] = exceptionHandlingCode; 

  // Otherwise, put exception handling code in hashmap, make new function 
  // with pseudo-entry, add exception handling code into new block, add new block
  // into new function, and link prevIns and pseudo-block with ET_EXCEPTION edge (set meta-data)
}

// Function Name: dynamicModification(cfgInstruction *currIns)
// Purpose: Function to setup CFG when we find dynamically modified code
// Returns: Nothing
// Exceptions/Errors from library calls: If alloc fails
void setCfgForDynamicModification(cfgInstruction *currIns, cfgState *cfgStructArg){
  // Pull out data from state struct
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  hashtable *htablefunc = cfgStructArg->curCfgStruct->htablefunc;
  cfgInstruction ** prevInsFromMap = cfgStructArg->prevInsFromMap;
  Function **curFunct = cfgStructArg->curFunct;
  Bbl **curBlock = cfgStructArg->curBlock;
  cfgStructArg->isSelfMod = 1;

  //printf("Dynamically modified code found at %" PRIx64" in phase %d \n", currIns->event.ins.addr, cfgStructArg->curCfgStruct->id);
  int tid = currIns->event.ins.tid;
  // Check to see if we need to split a block by looking at old instruction
  cfgInstruction *oldCurrIns = get_key_val(htablecfg, currIns->event.ins.addr);
  if(oldCurrIns != NULL){
    oldCurrIns->dynamic = 1;
  }
  int alreadySplit  = 0;
  // EDGE OUT OF BOT OF OLD DYNAMIC INS
  // Split the block for the dynamic edge to come out of the bottom of the previous version of the dynamic code
  if(oldCurrIns != NULL && oldCurrIns->next != NULL && compareInstructions(oldCurrIns, oldCurrIns->block->last) == 0){
    splitBlock(oldCurrIns, oldCurrIns->next, oldCurrIns->block, oldCurrIns->next->block, cfgStructArg); // Will be fall through
    // Note don't need to split this block twice if we find the next ins happens to be prevIns
    if(oldCurrIns->next == prevInsFromMap[tid]){
      alreadySplit = 1;
    }
    // Set this edge to a ghost edge (i.e. this block would be whole if not for dynamic mod)
    Edge *getSplit = existingEdge(oldCurrIns->block, oldCurrIns->block->succs->to, prevInsFromMap[tid], oldCurrIns, cfgStructArg->disState);
    assert(oldCurrIns->block->succs->next == NULL); // If we just split this block then it can't have two edges comming out
    getSplit->etype = ET_GHOST;
    oldCurrIns->block->fallThroughOut = getSplit;
  }

  // EDGE IN TOP OF DYNAMIC INS
  // If different blocks already: Grab the edge between the previous version of the dynamic code and the previous instruction and set to dynamic
  edge_type savedEdgeType = ET_NORMAL;
  if(oldCurrIns != NULL && alreadySplit == 0 && oldCurrIns->block != prevInsFromMap[tid]->block){
    Edge *isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, oldCurrIns->block, prevInsFromMap[tid], oldCurrIns, cfgStructArg->disState);
    updateOrCreateEdge(isThereAnEdge, prevInsFromMap[tid], oldCurrIns, cfgStructArg);
    isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, oldCurrIns->block, prevInsFromMap[tid], oldCurrIns, cfgStructArg->disState);
    isThereAnEdge->isDynamic = 1;
    savedEdgeType = isThereAnEdge->etype;
  }
  // If same block: Split the block and make edge between dynamic
  else if(oldCurrIns != NULL && oldCurrIns->block == prevInsFromMap[tid]->block){
    splitBlock(prevInsFromMap[tid], oldCurrIns, prevInsFromMap[tid]->block, oldCurrIns->block, cfgStructArg);
    // Add dynamic edge
    Edge *isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, oldCurrIns->block, prevInsFromMap[tid], oldCurrIns, cfgStructArg->disState);
    isThereAnEdge->isDynamic = 1;
    savedEdgeType = isThereAnEdge->etype;
    // Add ghost edge
    edgeTuple *edgeCol = addNewEdgeWithType(ET_GHOST, prevInsFromMap[tid]->block, oldCurrIns->block, cfgStructArg);
    Edge *ghost = edgeCol->succ;
    oldCurrIns->block->fallThroughIn = ghost;
  }
  // Otherwise its the first time we've seen this dynamic ins (i.e. dynamic generation). Need to get savedEdgeType manually...
  else {
    savedEdgeType = peekEdgeType(prevInsFromMap[tid], currIns, cfgStructArg);
  }

  //update curPhase's last
  cfgStructArg->curCfgStruct->last = prevInsFromMap[tid]->block->fun;
  // Create new CFG phase here and set curCFG to new phase
  cfg *newPhase = alloc(sizeof(cfg));
  newPhase->startingpoint = NULL;
  newPhase->last = NULL;
  newPhase->htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  newPhase->htablefunc = cfgStructArg->curCfgStruct->htablefunc;
  newPhase->nextPhase = NULL;
  newPhase->prevPhase = NULL;
  cfgStructArg->curCfgStruct->nextPhase = newPhase;
  cfgStructArg->numP++;
  newPhase->prevPhase = cfgStructArg->curCfgStruct;
  newPhase->id = (newPhase->prevPhase->id)+1;
  // Update htablefunc/cfg both local and cfgStruct
  freeMemTaintLabels(cfgStructArg->taintState);
  cfgStructArg->curCfgStruct = newPhase;
  //printf("New Phase: %d\n", newPhase->id);
  htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  htablefunc = cfgStructArg->curCfgStruct->htablefunc;

  // Put new instruction into hashmap
  put_key_val(htablecfg, currIns->event.ins.addr, (void *)currIns);
  cfgStructArg->numI++;
  currIns = get_key_val(htablecfg, currIns->event.ins.addr);
  // If we've seen this ins before, then copy over its block
  if(oldCurrIns != NULL){
    currIns->block = oldCurrIns->block;
    currIns->block->first = currIns;
    currIns->block->last = currIns;
    currIns->block->fun->cfgPhase = cfgStructArg->curCfgStruct;
    addToPhaseList(currIns->block->fun, cfgStructArg->curCfgStruct->id);
    currIns->block->cfgPhase = cfgStructArg->curCfgStruct;
    addToBlockPhaseList(currIns->block, cfgStructArg->curCfgStruct->id);
  }
  if(currIns->block == NULL){
    Bbl *blockInNewCFG = newBlock(NULL, currIns, cfgStructArg);
    currIns->block = blockInNewCFG;
    addInsIntoBlock(blockInNewCFG, currIns);
    Function *oldFunc = prevInsFromMap[tid]->block->fun;
    addBlockIntoFunction(oldFunc, blockInNewCFG);
  }

  if(oldCurrIns != NULL){
    curFunct[tid] = oldCurrIns->block->fun;
    curBlock[tid] = oldCurrIns->block;
    prevInsFromMap[tid] = oldCurrIns;
  }
  cfgStructArg->curCfgStruct->startingpoint = currIns->block->fun;
  cfgStructArg->curCfgStruct->last = currIns->block->fun;
  uint8_t updateCount = 0;
  if(prevInsFromMap[tid]->block->fun->exit != NULL){
    updateCount = 1;
  }
  finalizeCFG(cfgStructArg);

  // EDGE FROM OLD DYNAMIC TO EXIT BLOCK
  Edge *dynamicSourceEdge = NULL;
  dynamicSourceEdge = existingEdge(prevInsFromMap[tid]->block, prevInsFromMap[tid]->block->fun->exit, prevInsFromMap[tid], NULL, cfgStructArg->disState);
  assert(dynamicSourceEdge != NULL);
  dynamicSourceEdge->isDynamic = 1;
  if(updateCount){
    //dynamicSourceEdge->count++;
  }
  assert(prevInsFromMap[tid]->block->fun != NULL);

  // EDGE FROM ENTRY TO NEW DYNAMIC
  Edge *dynamicTargetEdge = searchThroughSuccessorsForDynamic(currIns->block->fun->entry, currIns->block);
  if(dynamicTargetEdge == NULL){
    edgeTuple *edgeCol = addNewEdgeWithType(ET_ENTRY, currIns->block->fun->entry, currIns->block, cfgStructArg);
    dynamicTargetEdge = edgeCol->succ;
  } else {
    // TODO: Else update shared dynamic target edge with new phase #/counts
    dynamicTargetEdge->count++;
  } 
  dynamicTargetEdge->isDynamic = 1;
  // EDGE BETWEEN PHASES (EXIT TO ENTRY)
  // Create a dynamic edge linking the two cfgs together
  // Use saved edge type and set pointers to source/target edges
  Edge *dynamicEdge = searchThroughSuccessorsWithType(savedEdgeType, prevInsFromMap[tid]->block->fun->exit, currIns->block->fun->entry);
  if(dynamicEdge == NULL){
    edgeTuple *edgeCol = addNewEdgeWithType(savedEdgeType, prevInsFromMap[tid]->block->fun->exit, currIns->block->fun->entry, cfgStructArg);
    dynamicEdge = edgeCol->succ;
  } else {
    // TODO: Else update shared dynamic target edge with new phase #/counts
    dynamicEdge->count++;
  }
  dynamicEdge->isDynamic = 1;
  dynamicEdge->dynamicSource = dynamicSourceEdge;
  dynamicEdge->dynamicTarget = dynamicTargetEdge;  
  // Reset all prevInsFromMaps and then set the prevInsFromMap for this thread then continue
  prevInsFromMap[tid] = currIns;
}

void taintMemDestinations(cfgState *cfgStructArg, cfgInstruction *curInstruction){
  // Get a new label to apply to all dest/sources
  uint64_t label = getNewLabel(cfgStructArg->taintState);
  InsInfo info;
  initInsInfo(&info);
  fetchInsInfo(cfgStructArg->readerState, &curInstruction->event.ins, &info);

  // Walk through read/write
  ReaderOp *op = info.readWriteOps;
  for(int i = 0; i < info.readWriteOpCnt; i++){
    // Look for mem ops
    if(op->type == MEM_OP){
      /*if(op->mem.size > 200){
        printf("Great: Ins %lx has mem size of %d\n", curInstruction->event.ins.addr, op->mem.size);
      }*/
      taintMem(cfgStructArg->taintState, op->mem.addr, op->mem.size, label);
    }
    op = op->next;
  }

  // Walk through dstOps
  op = info.dstOps;
  for(int i = 0; i < info.dstOpCnt; i++){
    // Look for mem ops
    if(op->type == MEM_OP){
      /*if(op->mem.size > 200){
        printf("Great: Ins %lx has mem size of %d\n", curInstruction->event.ins.addr, op->mem.size);
      }*/
      taintMem(cfgStructArg->taintState, op->mem.addr, op->mem.size, label);
    }
    op = op->next;
  }
}

// Function name: addInstructionToCFG(ReaderEvent *readEvent)
// Purpose: Add an instruction into the cfg
// Exceptions: If alloc fails due to a lack of memory
// Arguments: Pointer to the cfg struct that gets filled 
cfgInstruction *addInstructionToCFG(ReaderEvent *readEvent, cfgState *cfgStructArg){
  // Pull out the data we need from the cfgState struct
  cfgInstruction **prevInsFromMap = cfgStructArg->prevInsFromMap;
  Function **curFunct = cfgStructArg->curFunct;
  Bbl **curBlock = cfgStructArg->curBlock;
  hashtable *htablecfg = cfgStructArg->curCfgStruct->htablecfg;
  // Handle exceptions
  if (readEvent->type == EXCEPTION_EVENT){
    // We shouldn't get two exception events in a row
    assert(cfgStructArg->pendingException == NULL);
    cfgInstruction *anException = alloc(sizeof(cfgInstruction));
    //memcpy(&readEvent, &(anException->event), sizeof(ReaderEvent *));
    memcpy(&(anException->event), readEvent, sizeof(ReaderEvent));
    //anException->event = readEvent;
    cfgStructArg->pendingException = anException;
    return anException;
  }

  cfgInstruction *currIns = alloc(sizeof(cfgInstruction));
  currIns->keep = 2;
  currIns->dynamic = 0;
  currIns->fetched = 0;
  currIns->type = 0;
  currIns->unsignedImm = 0;
  currIns->prevVersion = NULL;
  currIns->block = NULL;
  currIns->phaseID = cfgStructArg->curCfgStruct->id;
  memcpy(&(currIns->event), readEvent, sizeof(ReaderEvent));
  // Taint the Instructions memoryDestinations to catch any writes
  //printf("%lx\n", currIns->event.ins.addr);
  taintMemDestinations(cfgStructArg, currIns);
  int tid = currIns->event.ins.tid;
  // Look to see if the last event added was an exception event
  if (cfgStructArg->pendingException != NULL){
    setCfgForException(currIns, cfgStructArg);
    cfgInstruction *currInsFromMap = get_key_val(htablecfg, currIns->event.ins.addr);
    currIns->block = currInsFromMap->block;
    return currIns;
  }

  // If this is the first instruction added to the cfg or the first time seeing this thread
  if (prevInsFromMap[tid] == NULL){
    assert(!isDynamicallyModifiedCode(currIns, cfgStructArg));
   // printf("tid is %d\n", tid);
    handleAppearanceOfNewThread(currIns, cfgStructArg);
    cfgInstruction *currInsFromMap = get_key_val(htablecfg, currIns->event.ins.addr);
    if(currInsFromMap->block->fun->cfgPhase != cfgStructArg->curCfgStruct){
      currInsFromMap->block->fun->cfgPhase = cfgStructArg->curCfgStruct;
      addToPhaseList(currInsFromMap->block->fun, cfgStructArg->curCfgStruct->id);
    }
    currIns->block = currInsFromMap->block;
    return currIns;
  }
  // If we have seen this instruction before (same address) but have different instruction bytes 
  // then we have found dynamically modified code
  if (isDynamicallyModifiedCode(currIns, cfgStructArg)) {
    setCfgForDynamicModification(currIns, cfgStructArg);
    htablecfg = cfgStructArg->curCfgStruct->htablecfg; 
    cfgInstruction *currInsFromMap = get_key_val(htablecfg, currIns->event.ins.addr);
    currIns->block = currInsFromMap->block;
    return currIns;
  }
 
  cfgInstruction *currInsFromMap = get_key_val(htablecfg, currIns->event.ins.addr);
  if(currInsFromMap == NULL){
    cfgStructArg->numI++;
  }
 
  // CASE 1: We have seen the instruction before and we should look at what kind of control flow/updates we will need to do
  if (currInsFromMap != NULL){
    // Update the phase value for functions/block if needed
    if(currInsFromMap->block->fun->cfgPhase != cfgStructArg->curCfgStruct){
      currInsFromMap->block->fun->cfgPhase = cfgStructArg->curCfgStruct;
      addToPhaseList(currInsFromMap->block->fun, cfgStructArg->curCfgStruct->id);
    }
    curBlock[tid] = currInsFromMap->block;
    curFunct[tid] = currInsFromMap->block->fun;
    // CASE 1.1: This is our first case, where we simply make a FALL-THROUGH EDGE between prevIns's and currIns's blocks
    Edge *isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, currInsFromMap->block, prevInsFromMap[tid], currInsFromMap, cfgStructArg->disState);
    if (prevInsFromMap[tid] != NULL && StraightLineCode(prevInsFromMap[tid]->event.ins, currInsFromMap->event.ins)){
      if (currInsFromMap->block->id != prevInsFromMap[tid]->block->id){
        // If the instruction is in a different block then we either need to
        // update an edge or create a new edge (if one doesn't exist)
        // Note, if we are comming from a block from a different phase then it should be a ghost edge here
        // i.e. straight line code, block was only split apart due to phase differences
        updateOrCreateEdge(isThereAnEdge, prevInsFromMap[tid], currInsFromMap, cfgStructArg);
        if(currInsFromMap->block->cfgPhase != cfgStructArg->curCfgStruct){
          isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, currInsFromMap->block, prevInsFromMap[tid], currInsFromMap, cfgStructArg->disState);  
          isThereAnEdge->etype = ET_GHOST;
        }
      }
      // Update phase for current block and then return
      if(currInsFromMap->block->cfgPhase != cfgStructArg->curCfgStruct){
        currInsFromMap->block->cfgPhase = cfgStructArg->curCfgStruct;
        addToBlockPhaseList(currInsFromMap->block, cfgStructArg->curCfgStruct->id);
      }
      prevInsFromMap[tid] = currInsFromMap;
      cfgInstruction *holder = currInsFromMap;
      currInsFromMap = NULL;

      return holder;
    } // End if (Fall through edge)
    // Update current block's phase
    if(currInsFromMap->block->cfgPhase != cfgStructArg->curCfgStruct){
      currInsFromMap->block->cfgPhase = cfgStructArg->curCfgStruct;
      addToBlockPhaseList(currInsFromMap->block, cfgStructArg->curCfgStruct->id);
    }
    // CASE 1.2: This is where we have CONTROL TRANSFER	
    // CASE 1.2.1: We have control transfer to the middle of a block
    if (compareInstructions(currInsFromMap, currInsFromMap->block->first) == 0){
      splitBlock(prevInsFromMap[tid], currInsFromMap, prevInsFromMap[tid]->block, currInsFromMap->block, cfgStructArg);
    // CASE 1.2.2: This is where the control transfer jumps to the start of currIns's block 
    } else {
      updateOrCreateEdge(isThereAnEdge, prevInsFromMap[tid], currInsFromMap, cfgStructArg);
      currInsFromMap->block->count++;
    } // End else (control transfer to beginning of block)
  
  // CASE 2: We have not seen this instruction before (determine if it goes in the current block or gets a new block/function)
  } else {
    put_key_val(htablecfg, currIns->event.ins.addr, (void *)currIns);
    currInsFromMap = get_key_val(htablecfg, currIns->event.ins.addr);
    // CASE 2.1: Add current ins into previous block (straightline code)
    if (prevInsFromMap[tid] != NULL 
        && StraightLineCode(prevInsFromMap[tid]->event.ins, currInsFromMap->event.ins)
        && !(isControlTransfer(prevInsFromMap[tid], cfgStructArg->disState)) ){
      // If we have already completed this block in another phase
      // Must have at least one other phase before we have to worry about this
      if(prevInsFromMap[tid]->block->succs != NULL && prevInsFromMap[tid]->block->phases != NULL && prevInsFromMap[tid]->block->phases->next != NULL){
        // If we have to split the old block (i.e. adding to the middle)
        if(prevInsFromMap[tid]->next != NULL){
          cfgInstruction *next = prevInsFromMap[tid]->next;
          splitBlock(prevInsFromMap[tid], next, prevInsFromMap[tid]->block, next->block, cfgStructArg);
          Edge *isThereAnEdge = existingEdge(prevInsFromMap[tid]->block, next->block, prevInsFromMap[tid], next, cfgStructArg->disState);
          isThereAnEdge->etype = ET_GHOST;
          // Remove this current phase as being part of the second half's phase list
          isThereAnEdge->phases = NULL;
          next->block->phases = NULL;
          phaseList *walkP = prevInsFromMap[tid]->block->phases;
          while(walkP != NULL){
            if(walkP->phase != cfgStructArg->curCfgStruct->id){
              addToEdgePhaseList(isThereAnEdge, walkP->phase);
              addToBlockPhaseList(next->block, walkP->phase);
            }
            walkP = walkP->next;
          }
          // Reset the last phase seen for the split block's second half
          if(next->block->succs != NULL){
            next->block->cfgPhase = next->block->succs->to->cfgPhase;
          }
        }
        Bbl *newBbl = newBlock(prevInsFromMap[tid], currInsFromMap, cfgStructArg);
        addInsIntoBlock(newBbl, currInsFromMap);
        curBlock[tid] = newBbl;
        curFunct[tid] = prevInsFromMap[tid]->block->fun;
        addNewEdgeWithType(ET_GHOST, prevInsFromMap[tid]->block, currInsFromMap->block, cfgStructArg); 
      } else {
        addInsIntoBlock(prevInsFromMap[tid]->block, currInsFromMap);
        assert(prevInsFromMap[tid]->block->id == currInsFromMap->block->id);
      }
      curFunct[tid] = prevInsFromMap[tid]->block->fun;
      currInsFromMap->block->fun = curFunct[tid];
    // CASE 2.2: Make a new block for curIns
    } else{
      Bbl *newBbl = newBlock(prevInsFromMap[tid], currInsFromMap, cfgStructArg);
      addInsIntoBlock(newBbl, currInsFromMap);
      curBlock[tid] = newBbl;
      curFunct[tid] = prevInsFromMap[tid]->block->fun; 
      createEdge(prevInsFromMap[tid], currInsFromMap, cfgStructArg); 
      // If newBbl has its function pointer set then the previous instruction was a call.
      // If the pointer isn't set then newBbl belongs in the same function as the previous ins
      if (newBbl->fun == NULL){
        addBlockIntoFunction(curFunct[tid], newBbl);
      } 
    } // End else (when we make a new block)
  } // End else (when we haven't seen the instruction before)

  prevInsFromMap[tid] = currInsFromMap;
  cfgInstruction *holder = currInsFromMap;
  currInsFromMap = NULL;
  return(holder);
}

//prototype for function used in closeCFG
void mergeConditionals(cfgState *cfgStructArg);

// Function name: closeCFG
// Purpose: Finish up the cfg, adding the exiting pseudoblock and wrapping up loose ends
// Exceptions: If alloc fails due to a lack of memory the program will throw an error and exit
// Arguments: None
void finalizeCFG(cfgState *cfgStructArg){
  // Pull out data from cfg state struct  
  cfgInstruction **prevInsFromMap = cfgStructArg->prevInsFromMap;
  int numThreads = cfgStructArg->numThreads;
  Bbl **curBlock = cfgStructArg->curBlock;
  Function **curFunct = cfgStructArg->curFunct;

  // Walk through all of our threads and tie them up
  int i;
  for(i = 0; i < numThreads; i++){
    if(curBlock[i] == NULL) {
        continue;
    }
    // If we find a exit edge then we don't have to do anything
    Edge *walk = NULL;
    for(walk = curBlock[i]->succs; walk != NULL; walk = walk->next){
      if (walk->etype == ET_EXIT){
        walk->count++;
        if(walk->cfgPhase != cfgStructArg->curCfgStruct){
          addToEdgePhaseList(walk, cfgStructArg->curCfgStruct->id);
          walk->cfgPhase = cfgStructArg->curCfgStruct;
        }
        break;
      }
    }
    // Look to see what walk is
    if (walk == NULL){
      // If there is no exit edge then see whether there is an exit block already for that function
      Bbl *anExit = curBlock[i]->fun->exit;
      // Make an exit block if one doesn't exist
      if (anExit == NULL){
        // Make a new exit block since we are finishing the cfg at this point
        anExit = newBlock(prevInsFromMap[i], NULL, cfgStructArg);
        anExit->btype = BT_EXIT;
        anExit->prev = curBlock[i];
        anExit->next = NULL;
        anExit->fun = curBlock[i]->fun;

        // Add the exit block to the end of the current functions list of blocks
        curFunct[i]->last->next = anExit;
        curFunct[i]->last = anExit;
        curFunct[i]->exit = anExit;
      }
      // Create a new edge for the exit block
      addNewEdgeWithType(ET_EXIT, curBlock[i], anExit, cfgStructArg);
    }
  }
  if (cfgStructArg->isSelfMod == 0){
    mergeConditionals(cfgStructArg);
  }
}

void checkTargetAddress(cfgInstruction *jmp, cfgState *cfgStructArg){
  cfgInsInfo iinfo;
  fetchInsInfoNoReaderState(cfgStructArg->disState, jmp, &iinfo);
  
  cfgInstruction *body = NULL;
  if(jmp->type == UNSIGNED_IMM_OP){
    // Get the address of the target instruction to see if it exists
    body = get_key_val(cfgStructArg->curCfgStruct->htablecfg, (jmp->event.ins.addr + jmp->event.ins.binSize + jmp->unsignedImm));
  }
  else {
    body = get_key_val(cfgStructArg->curCfgStruct->htablecfg, (jmp->event.ins.addr + jmp->event.ins.binSize + jmp->signedImm));
  }
  // Check to see if we need to split the block
  // Note this will add an edge already from jmp->block to body->block
  if(body != NULL && compareInstructions(body, body->block->first) == 0){
    splitBlock(body->prev, body, NULL, body->block, cfgStructArg);
  }
  if(body != NULL){
    Edge *temp = existingEdge(jmp->block, body->block, NULL, NULL, cfgStructArg->disState);
    if(temp == NULL){
      addNewEdgeWithType(ET_TRUE, jmp->block, body->block, cfgStructArg);
    }
  }
}

void checkFallThrough(cfgInstruction *jmp, cfgState *cfgStructArg){
  cfgInstruction *body = get_key_val(cfgStructArg->curCfgStruct->htablecfg, (jmp->event.ins.addr + jmp->event.ins.binSize));
  if(body != NULL){
    addNewEdgeWithType(ET_FALSE, jmp->block, body->block, cfgStructArg);
  }
}

void checkConditional(Bbl *walkBlock, cfgState *cfgStructArg){
  // See if both edges are taken
  int trueEdge = 0;
  int falseEdge = 0;

  Edge *walkEdge = NULL;
  for (walkEdge = walkBlock->succs; walkEdge != NULL; walkEdge = walkEdge->next){
    if (walkEdge->etype == ET_TRUE){
      trueEdge = 1;
    }
    else if (walkEdge->etype == ET_FALSE){
      falseEdge = 1;
    }
  }
  assert(trueEdge || falseEdge);
  // See if both edges present
  if (trueEdge && falseEdge){
    return;
  }
  else if (falseEdge){ // No true edge present
    checkTargetAddress(walkBlock->last, cfgStructArg);
  } else { // No false edge present
    checkFallThrough(walkBlock->last, cfgStructArg);
  }
}

void mergeConditionals(cfgState *cfgStructArg){
  Function *walkFun = NULL;
  for (walkFun = cfgStructArg->curCfgStruct->startingpoint; walkFun != NULL; walkFun = walkFun->next){
    Bbl *walkBlock = NULL;
    for (walkBlock = walkFun->first; walkBlock != NULL; walkBlock = walkBlock->next){
      // Look out for conditional blocks
      if (walkBlock->btype == BT_COND){
        checkConditional(walkBlock, cfgStructArg);
      }
    }
  }
}

void printCollisions(cfgState *cfgStructArg){
  hashtable *ht = cfgStructArg->curCfgStruct->htablecfg;
  printf("Inserts: %d Collisions: %d Max Depth: %d\n", ht->ins, ht->col, ht->maxD);
}

void printNumBlocks(cfgState *cfgStructArg){
  printf("Num Blocks: %ld\n", cfgStructArg->numB);
}

void printNumEdges(cfgState *cfgStructArg){
  printf("Num Edges: %ld\n", cfgStructArg->numE);
}

void printNumPhases(cfgState *cfgStructArg){
  printf("Num Phases: %ld\n", cfgStructArg->numP);
}

void printNumIns(cfgState *cfgStructArg){
  printf("Num Ins: %ld\n", cfgStructArg->numI);
}

// Function Name: freeCFG
// Purpose: Free the CFG struct and all related blocks, edges, and instructions
// Exceptions/errors: None
void freeCFG(cfg *cfgStruct, cfgState *cfgStructArg){
  Function *tempf = cfgStruct->startingpoint;
  while(tempf != NULL){
    Function *fholder = tempf->next;
    freeFunction(tempf); 
    tempf = fholder;
  }
  free(cfgStruct); // Free the holder struct for the cfg
  freeTable(cfgStructArg->curCfgStruct->htablecfg); // Free the supporting hash table
}

cfg *getStartCFG(cfgState *state) {
    return state->startingPhase;
}
