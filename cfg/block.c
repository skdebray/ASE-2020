/*
* File: block.c
* Author: Jesse Bartels
* Description: Any and all functions related to manipulating blocks are located in this file
*              --Creating new blocks
*              --Spliting a block into two new blocks
*              --Freeing blocks (soon)
*/
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "cfgState.h"
#include "cfg.h"
#include "utils.h"
#include "block.h"
#include "edge.h"

// Global table for block type lookups
// Global value for block types
#define BLOCKTYPES 12
bbl_type blockTypeFromEdgeType[BLOCKTYPES] = {BT_NORMAL, // ET_NORMAL(1)
                                                   BT_COND,   // ET_TRUE(2)
                                                   BT_COND,   // ET_FALSE(3)
                                                   BT_UNCOND, // ET_JMP(4)
                                                   BT_UNCOND, // ET_INDIR(5)
                                                   BT_CALL,   // ET_CALL(6)
                                                   BT_RET,    // ET_RET(7)
                                                   BT_ENTRY,  // ET_ENTRY(8)
                                                   BT_EXIT,   // ET_EXIT(9)
                                                   BT_UNKNOWN,// ET_LINK(10)
                                                   BT_UNKNOWN,// ET_EXCEPTION(11)
                                                   BT_UNKNOWN,// ET_DYNAMIC(12)
                                                   };


// Function Name: newBlock
// Purpose: Create a new "default" block, allocate space, and return it
// Returns: Pointer to a new block (if alloc succceeds)
// Exceptions/Errors from library calls: Alloc (from utils.c) can throw an error if there is no more memory
Bbl *
newBlock (cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg)
{
    Bbl *newBlock = alloc (sizeof (Bbl));
    cfgStructArg->blockID++;
    newBlock->id = cfgStructArg->blockID;
    newBlock->btype = BT_NORMAL;
    newBlock->first = NULL;
    newBlock->last = NULL;
    newBlock->preds = NULL;
    newBlock->succs = NULL;
    newBlock->fun = NULL;
    newBlock->count = 1;
    newBlock->flags = 0;
    newBlock->prev = NULL;
    newBlock->next = NULL;
    newBlock->auxinfo = NULL;
    newBlock->visited = 0;
    newBlock->cfgPhase = cfgStructArg->curCfgStruct;
    newBlock->phases = NULL;
    addToBlockPhaseList(newBlock, cfgStructArg->curCfgStruct->id);
    cfgStructArg->numB++;
    return newBlock;
}

void addInsIntoBlock(Bbl *blockToInsertInto, cfgInstruction *currIns){
  // Case where its a new block
  if (blockToInsertInto->first == NULL){
    blockToInsertInto->first = currIns;
  } else {
    blockToInsertInto->last->next = currIns;
  }
  currIns->prev = blockToInsertInto->last;
  blockToInsertInto->last = currIns;
  currIns->next = NULL; 
  currIns->block = blockToInsertInto;
}

void determineBlockTypeFromEdgeType(edge_type etype, Bbl *block){
  block->btype = blockTypeFromEdgeType[etype];
}

// Update phases that this block is in
void addToBlockPhaseList(Bbl *blockArg, int phaseID){
  phaseList *nu = alloc(sizeof(phaseList));
  nu->phase = phaseID;
  nu->next = blockArg->phases;
  blockArg->phases = nu;
}

// Tests if given function is in a phase
int bblPartOfPhase(Bbl *blockArg, int phaseID){
  phaseList *temp = blockArg->phases;
  while(temp != NULL){
    if(temp->phase == phaseID){
      return 1;
    }
    temp = temp->next;
  }
  return 0;
}

// Function Name: splitBlock
// Purpose: Function that takes a block and splits it into two separate blocks, linking together the neccessary lists
// Returns: Void
// Exceptions/errors: Alloc can throw an error if there is no space left to use for allocation
void
splitBlock (cfgInstruction * prevIns, cfgInstruction * currIns, Bbl * prevBbl,
	    Bbl * currBbl, cfgState *cfgStructArg)
{
    // Pull out data from saved cfg state
    Bbl **curBlock = cfgStructArg->curBlock;
    Function **curFunct = cfgStructArg->curFunct;
    curFunct[currIns->event.ins.tid] = currIns->block->fun;
    // First, create a new block in which the second half of instructions will be places
    Bbl *currBblTwo = newBlock (prevIns, currIns, cfgStructArg);
    currBblTwo->btype = currBbl->btype;
    // succs edge is a fallthrough edge, set below
    currBblTwo->fun = curFunct[currIns->event.ins.tid];

    // Set the successor and predecesssor edges for the new block/old block   
    // New block's list of successors is the old block's successor list and the new block's predecessor list is a 
    // new list with only the fall through edge in it
    currBblTwo->succs = currBbl->succs;
    currBblTwo->preds = NULL;
    currBblTwo->fallThroughOut = currBbl->fallThroughOut;
    currBbl->fallThroughOut = NULL;
    currBblTwo->fallThroughIn = NULL;
    
    // Update phase info in new block
    currBblTwo->cfgPhase = currBbl->cfgPhase;
    currBblTwo->phases = NULL;
    phaseList *walkP = currBbl->phases;
    while(walkP != NULL){
      addToBlockPhaseList(currBblTwo, walkP->phase);
      walkP = walkP->next;
    }

    // Old block's predecessors' stay the same
    // the sucessors list becomes a new list (will add a fall through edge in it later)
    currBbl->succs = NULL;

    // Add the split blocks into our linked list of blocks
    if (currBbl->next != NULL){
      currBbl->next->prev = currBblTwo;
    }
    currBblTwo->next = currBbl->next;
    currBbl->next = currBblTwo;
    currBblTwo->prev = currBbl;
    if (curFunct[currIns->event.ins.tid]->last == currBbl) {
      curFunct[currIns->event.ins.tid]->last = currBblTwo;
    }
    
    // Update all edges in our currBblTwo's successsor list 
    // Go through the new list of successsor edges that was handed to the second block and update the "from" pointer
    Edge *update = NULL;
    for(update = currBblTwo->succs; update != NULL; update = update->next){
      update->from = currBblTwo;
    }
    // Update all the predecessor edges for all of the successor's of currBblTwo.
    Edge *predB = NULL; // Look through all of our successors
    for(predB = currBblTwo->succs; predB != NULL; predB = predB->next){
      Edge *reset = NULL; // Look through each successor's predecessor to find the edges that point to currBbl
      for(reset = predB->to->preds; reset != NULL; reset = reset->next){
        // If the predecessor edge was pointing at the old currBbl
        if (reset->from->id == currBbl->id){
          // Then have it now point at the last half of the split block, currBblTwo
          reset->from = currBblTwo;
        }
      }
    }
    
    //Add fall through edge between bbl0 and bbl1
    addNewEdgeWithType(ET_NORMAL, currBbl, currBblTwo, cfgStructArg);
    currBbl->btype = BT_NORMAL;
    // Look for the current instruction in the block's list of instructions
    cfgInstruction *temp = currBbl->first;
    while (temp != NULL && temp->next != NULL && compareInstructions(temp->next, currIns) != 1) {
      temp = temp->next;
    }
    // If we walked through our list and never found a next pointer that pointed to currIns then there is a serious issue
    assert(temp != NULL && temp->next != NULL);

    // Now cut the list, updating the second half's block pointer
    // temp->next currently points at currIns as we just finished the loop above
    currBblTwo->first = temp->next;
    // Cut the old linked list in CurrBbl and update the last pointers
    temp->next = NULL;
    currBbl->last = temp;

    // If this is a case where prevIns was null due to a thread swap, then assign prevIns here
    if (prevIns == NULL){
      prevIns = temp;
    }
    // Walk through the second half of the list, updating the block pointer 
    cfgInstruction *walk = NULL;
    for(walk = currBblTwo->first; walk != NULL; walk->block = currBblTwo, walk = walk->next){
      if (walk->next == NULL){
        currBblTwo->last = walk;
      }
    }

    // Make apropriate type of edge between previous and current instruction
    curBlock[currIns->event.ins.tid] = currBblTwo;
    Edge *mayExist = existingEdge(prevIns->block, currIns->block, NULL, NULL, cfgStructArg->disState);
    if(mayExist == NULL){
      createEdge (prevIns, currIns, cfgStructArg);
      // If we are splitting this block then ensure that all other phases using this block 
      // get a ghost edge (i.e. the block is not split for other phases)
      // Note that we can use all the phases present in the last ins of the first half of the split block
      // to figure out which phases to add to the ghost edge
      if(prevIns->block->phases != NULL && prevIns->block->phases->next != NULL){ // at least one other phase
        edgeTuple *edgeCol = addNewEdgeWithType(ET_GHOST, prevIns->block, currIns->block, cfgStructArg);
        Edge *ghostSucc = edgeCol->succ;
        ghostSucc->phases = NULL;
        Edge *ghostPred = edgeCol->pred;
        ghostPred->phases = NULL;
        // Walk through all phases that use this block and update the ghost edge
        phaseList *temp = prevIns->block->phases;
        while(temp != NULL){
          if(temp->phase != cfgStructArg->curCfgStruct->id){
            addToEdgePhaseList(ghostSucc, temp->phase);
            addToEdgePhaseList(ghostPred, temp->phase);
          }
          temp = temp->next;
        }
      }
    }
    currBblTwo->count = currBblTwo->count + 1;
}

// Function Name: freeBlock
// Purpose: Free the blockand the list of instructions in it
// Exceptions/Errors: None
// Arguments: Pointer to the block to be freed
void freeBlock(Bbl *block){
  // Go through the list of instructions and then free the block
  cfgInstruction *tempi = block->first;
  // Free the cfgInstruction structs within the block
  while(tempi != NULL){
    cfgInstruction *iholder = tempi->next;
    free(tempi); // Free the cfgInstructon struct
    tempi = iholder;
  }
  // Free the predecessor edges for a block
  Edge *tempp = block->preds;
  while(tempp != NULL){
    Edge *pholder = tempp->next;
    free(tempp);
    tempp = pholder;
  }
  // Free the successor edges for a block
  Edge *temps = block->succs;
  while(temps != NULL){
    Edge *sholder = temps->next;
    free(temps);
    temps = sholder;
  }
  // Finally free the block itself
  free(block);
}
