/*
* File: function.c
* Author: Jesse Bartels
* Description: All functions related to creating and manipulating functions go in this file
*
*/
#include <stdio.h>
#include <stdlib.h>
#include "cfgState.h"
#include "cfg.h"
#include "edge.h"
#include "utils.h"
#include "block.h"
#include "function.h"
#include <assert.h>

// Function Name: newFunction(cfgInstruction * currIns)
// Purpose: Create functions (reduce amount of duplicated code)
// Returns: Pointer to a new function if malloc is successful
// Parameters: The current and previous instruction
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory
Function *
newFunction (cfgInstruction * currIns, cfgState *cfgStructArg)
{
    Function *newFunct = alloc (sizeof (Function));
    cfgStructArg->functID++;
    //newFunct->id = cfgStructArg->functID;
    if(currIns != NULL){
      newFunct->id = currIns->event.ins.fnId;
      newFunct->name = cfgStructArg->strTable+(currIns->event.ins.fnId);
    }
    newFunct->first = NULL;
    newFunct->last = NULL;
    newFunct->entry = NULL;
    newFunct->exit = NULL;
    newFunct->count = 0;
    newFunct->prev = NULL;
    newFunct->next = NULL;
    newFunct->auxinfo = NULL;
    newFunct->cfgPhase = cfgStructArg->curCfgStruct;
    newFunct->phases = NULL;
    addToPhaseList(newFunct, cfgStructArg->curCfgStruct->id);
    return newFunct;
}

void addToPhaseList(Function *func, int phaseID){
  phaseList *nu = alloc(sizeof(phaseList));
  nu->phase = phaseID;
  nu->next = func->phases;
  func->phases = nu;
}

// Tests if given function is in a phase
int funPartOfPhase(Function *func, int phaseID){
  phaseList *temp = func->phases;
  while(temp != NULL){
    if(temp->phase == phaseID){
      return 1;
    }
    temp = temp->next;
  }
  return 0;
}

void updateEntryExitBlocks(Bbl *oldBlock, Bbl *newBlock){
  // Change all successors's predecessors to point at new target
  Edge *oldSuccs = NULL;
  for(oldSuccs = oldBlock->succs; oldSuccs != NULL; oldSuccs = oldSuccs->next){
    Edge *changeTarget = NULL;
    for(changeTarget = oldSuccs->to->preds; changeTarget != NULL; changeTarget = changeTarget->next){
      if(changeTarget->from->id == oldBlock->id){
        changeTarget->from = newBlock;
      }
    }
  }

  // Walk through all successors and move them to the new entry block
  Edge *moveOver = oldBlock->succs;
  while(moveOver != NULL){
    // Save the next value
    Edge *nextUp = moveOver->next;
    moveOver->from = newBlock;
    // Add into other's successor list
    addEdge(&(newBlock->succs), moveOver);
    moveOver = nextUp;
  }

  // Change all predecessor's successors to point at new target
  Edge *oldPreds = NULL;
  for(oldPreds = oldBlock->preds; oldPreds != NULL; oldPreds = oldPreds->next){
    Edge *changeTarget = NULL;
    for(changeTarget = oldPreds->from->succs; changeTarget != NULL; changeTarget = changeTarget->next){
      if(changeTarget->to->id == oldBlock->id){
        changeTarget->to = newBlock;
      }
    }
  }

  // Walk through all predecessors and move them to the new entry block
  moveOver = oldBlock->preds;
  while(moveOver != NULL){
    // Save the next value
    Edge *nextUp = moveOver->next;
    moveOver->to = newBlock;
    // Add into other's predecessor list
    addEdge(&(newBlock->preds), moveOver);
    moveOver = nextUp;
  }
  
}

void mergeFunctions(Function *merge, Function *mergeInto, Function *curFunct, cfgState *cfgStructArg){
  //printf("Merging %d into %d\n", merge->id, mergeInto->id);
  Bbl *oldEntry = merge->entry;
  Bbl *newEntry = mergeInto->entry;
  Bbl *oldExit = merge->exit;
  Bbl *newExit = mergeInto->exit;

  updateEntryExitBlocks(oldEntry, newEntry);
  if(oldExit != NULL && newExit != NULL){
    updateEntryExitBlocks(oldExit, newExit);
  } else if (newExit == NULL){
    mergeInto->exit = oldExit; // If there is only one exit block then use that one
  }

  // Walk through every block and change fn pointer
  Bbl *walk = NULL;
  for(walk = oldEntry; walk->next != NULL; walk = walk->next){
    walk->fun = mergeInto;
  }
  //printf("Function pointers updated\n");
  // Append linked list of blocks in old fn onto new
  walk->fun = mergeInto;
  walk->next = newEntry->next;
  newEntry->next = oldEntry->next;

  // Remove merge from linked list of blocks
  if(merge->prev != NULL){
    assert(merge->prev->next == merge);
    merge->prev->next = merge->next;
  }
  // Update CurFunct
  curFunct = mergeInto;
  // Update cfgs last if necessary
  if(cfgStructArg->curCfgStruct->last == merge){
    cfgStructArg->curCfgStruct->last = mergeInto;
  }
}

void addBlockIntoFunction(Function *fun, Bbl *block){
  if (fun->first == NULL){
    fun->first = block;
    fun->last = block;
    fun->entry = block;
  } else {
    fun->last->next = block;
    block->prev = fun->last;
    fun->last = block;
  }
  block->fun = fun;
  // Determine if we need to set any of the function's entry/exit pointers
  if (block->btype == BT_ENTRY){
    fun->entry = block;
  }
  if (block->btype == BT_EXIT){
    fun->exit = block;
  }
}

// Function Name: newFunctionWithPseudoEntry(cfgInstruction * currIns)
// Purpose: Create functions with an entry pseudo-block
// Returns: Pointer to a new function if malloc is successful
// Parameters: The current and previous instruction
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory
Function *
newFunctionWithPseudoEntry (cfgInstruction * currIns, cfgState *cfgStructArg)
{
    Function *newFunct = newFunction(currIns, cfgStructArg);
    Bbl *entry = newBlock (NULL, currIns, cfgStructArg);
    entry->btype = BT_ENTRY;
    addBlockIntoFunction(newFunct, entry);
    return newFunct;
}

void insertIntoFunctionList(cfg *cfg, Function *fun){
  // First time insert
  if(cfg->startingpoint == NULL){
    cfg->startingpoint = fun;
    cfg->last = fun;
    return;
  }
  fun->next = cfg->last->next;
  if (cfg->last->next != NULL){
    cfg->last->next->prev = fun;
  }
  cfg->last->next = fun;
  fun->prev = cfg->last;
  cfg->last = fun;
  fun->cfgPhase = cfg;
}

// Function Name: pushOntoFunctionStack
// Purpose: Push a return address onto the function stack
// Parameters: Previous and current instruction, along with state arg
// Returns: Nothing
// Exceptions/Errors: Alloc can fail
void pushOntoFunctionStack(cfgInstruction *prevIns, cfgInstruction *currIns, cfgState *cfgStructArg){
  // Pull stack from saved cfg state
  cfgStack **stack = cfgStructArg->stack;
  // Create a new pointer to push on the stack
  cfgStack *val = alloc (sizeof (cfgStack));
  // Return address (for when we finish the call) is right after the address of the call instruction
  val->addr =prevIns->event.ins.addr + prevIns->event.ins.binSize;
  // Save the block that the call instruction lives in (we will need it to create edges when we return)
  val->block = prevIns->block;
  // Push call instruction onto the stack
  val->next = stack[currIns->event.ins.tid];
  stack[currIns->event.ins.tid] = val;
  //printf("%llx pushed on stack from %llx\n", val->addr, prevIns->event.ins.addr);
}

// Function Name: freeFunction(Function *fun)
// Purpose: Free a function, along with all the blocks associated with the function
// Parameters: Function struct to free
// Returns: Nothing
// Exceptions/Errors: None
void freeFunction(Function *fun){
  // Go through the list of blocks and the free the function
  Bbl *tempb = fun->first;
  while(tempb != NULL){
    Bbl *bholder = tempb->next;
    freeBlock(tempb);
    tempb = bholder;
  }
  free(fun);
}
