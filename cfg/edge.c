/*
* File: edge.c
* Author: Jesse Bartels
* Description: Collections of functions related to edges in cfg tool
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "cfgState.h"
#include "cfg.h"
#include "utils.h"
#include "controlTransfer.h"
#include "hashtable.h"
#include "block.h"
#include "function.h"
#include "edge.h"
#include <assert.h>

//int stackSize = 0;
//InsInfo edgeInfo;

// Function Name: newEdge(cfgInstruction *prevIns, cfgInstruction *currIns)
// Purpose: Modular function to create edges (so we don't duplicate code)
// Parameters: The current instruction, previous instruction.
// Returns: Pointer to a new edge if malloc is successful
// Exceptions/Errors from library calls: Alloc will throw an error if out of memory
Edge *
newEdge (cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg)
{
    Edge *edg = (Edge*)alloc (sizeof (Edge));
    cfgStructArg->edgeID++;
    cfgStructArg->numE++;
    edg->id = cfgStructArg->edgeID;
    edg->from = NULL;
    edg->to = NULL;
    edg->count = 1;
    edg->flags = 0;
    edg->prev = NULL;
    edg->next = NULL;
    edg->isDynamic = 0;
    edg->dynamicSource = NULL;
    edg->dynamicTarget = NULL;
    edg->auxinfo = NULL;
    edg->visited = 0;
    edg->alreadyPrintedInAnotherPhase = 0;
    edg->cfgPhase = cfgStructArg->curCfgStruct;
    edg->phases = NULL;
    addToEdgePhaseList(edg, cfgStructArg->curCfgStruct->id);
    return edg;
}

// Function Name: newEdge(edge_type, Bbl *from, Bbl *to)
// Purpose: Createan edge with the given type
// Parameters: Edge type, block from and block to
// Returns: Pointer to a new edge if malloc is successful
// Exceptions/Errors from library calls: Alloc can error out
edgeTuple *
addNewEdgeWithType(edge_type type, Bbl *from, Bbl *to, cfgState *cfgStructArg){
  Edge *predEdge = newEdge(NULL, NULL, cfgStructArg);
  predEdge->etype = type;
  predEdge->from = from;
  predEdge->to = to;

  Edge *predEdgeCopy = newEdge(NULL, NULL, cfgStructArg);
  int id = predEdgeCopy->id;
  predEdgeCopy = (Edge*)memcpy(predEdgeCopy, predEdge, sizeof(Edge));
  predEdgeCopy->id = id;

  // Add our edge and its copy to predecessor/successsor edges
  addEdge(&(from->succs), predEdge);
  addEdge(&(to->preds), predEdgeCopy);

  edgeTuple *toReturn = (edgeTuple*)alloc(sizeof(edgeTuple));
  toReturn->pred = predEdgeCopy;
  toReturn->succ = predEdge;
  return toReturn;
}

void addToEdgePhaseList(Edge *edg, int phaseID){
  phaseList *nu = alloc(sizeof(phaseList));
  nu->phase = phaseID;
  nu->next = edg->phases;
  edg->phases = nu;
}

// Tests if given function is in a phase
int edgPartOfPhase(Edge *edg, int phaseID){
  phaseList *temp = edg->phases;
  while(temp != NULL){
    if(temp->phase == phaseID){
      return 1;
    }
    temp = temp->next;
  }
  return 0;
}


// Function: addEdge(Edge **listOfEdges, Edge *newEdge)
// Purpose: Add an edge to a linked list of successor/predecessor edges
// Parameters: The edge and list of edges in which to add the edge in
// returns: Nothing
void
addEdge (Edge ** listOfEdges, Edge * newEdge)
{
    if (*listOfEdges != NULL) {
        newEdge->next = *listOfEdges;
        (*listOfEdges)->prev = newEdge;
    }
    *listOfEdges = newEdge;
}

// Function Name: createEdge(cfgInstruction *prevIns, cfgInstruction *currIns)
// Purpose: Function that looks at the previous/current instruction in byte form to determine what kind of edge must be
//          created between curIns and prevIns. In all cases prevIns is the instruction that makes the jump/call/return.
//          currIns is the first instruction right after a jump/call/ret.
// Parameters: The current instruction, previous instruction.
// Returns: Nothing, creates edge between the necessary blocks
// Exceptions/Errors from library calls: Alloc will crash if we do not have enough memory
void
createEdge (cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg)
{
    // Pull out data from saved cfg state
    Bbl **curBlock = cfgStructArg->curBlock;
    Function **curFunct = cfgStructArg->curFunct;
    cfgStack **stack = cfgStructArg->stack;
    hashtable *htablefunc = cfgStructArg->curCfgStruct->htablefunc;

    // Grab the opcode of prevIns to find out what the instruction was
    uint8_t *bytes = prevIns->event.ins.binary;
    cfgInsInfo edgeInfo;
    fetchInsInfoNoReaderState(cfgStructArg->disState, prevIns, &edgeInfo);
    xed_iclass_enum_t instruc = edgeInfo.insClass;

    // Check to see if we need to merge functions
    if(currIns->block->fun != NULL && (isConditionalJump (instruc) || isUnconditionalJump (instruc)) && currIns->block->fun->id != prevIns->block->fun->id){
      //printf("Jump into another function from %" PRIx64" \n", prevIns->event.ins.addr);
      // Need to merge function
      mergeFunctions(prevIns->block->fun, currIns->block->fun, curFunct[currIns->event.ins.tid], cfgStructArg);
    }
    if (isConditionalJump (instruc)) {
        edge_type edgeType = StraightLineCode(prevIns->event.ins, currIns->event.ins) ? ET_FALSE : ET_TRUE;
        determineBlockTypeFromEdgeType(edgeType, prevIns->block);
	addNewEdgeWithType(edgeType, prevIns->block, currIns->block, cfgStructArg);
	return;
    } 
    else if (isUnconditionalJump (instruc)) {
        // Default unconditional edge type is JMP, unless proven to be an indirect jump
        edge_type edgeType = (bytes[0]==255) ? ET_INDIR : ET_JMP;
	determineBlockTypeFromEdgeType(edgeType, prevIns->block);
	addNewEdgeWithType(edgeType, prevIns->block, currIns->block, cfgStructArg);
	return;
    } 
    else if (isCall (instruc)) {
	if (positionIndepCode (prevIns, currIns, cfgStructArg->disState)) {
	    // Do not do anything if PIC
	    return;
	}
	// Otherwise handle the call
	pushOntoFunctionStack(prevIns, currIns, cfgStructArg);
	determineBlockTypeFromEdgeType(ET_CALL, prevIns->block);
	// Check to see if the ins's functions exists already, create one if it doesn't
        if (currIns->block->fun == NULL) {
	    Function *newFunct = newFunctionWithPseudoEntry(currIns, cfgStructArg);
            put_key_val(htablefunc, newFunct->id, (void *)newFunct);

            insertIntoFunctionList(cfgStructArg->curCfgStruct, newFunct);
	    addBlockIntoFunction(newFunct, currIns->block);
            addNewEdgeWithType(ET_ENTRY, newFunct->entry, currIns->block, cfgStructArg);
	}
        else {
            // We've seen the function before, see if there is an edge
            Edge *mayExist = existingEdge (currIns->block->fun->entry, currIns->block, NULL, NULL, cfgStructArg->disState);
            if (mayExist != NULL){
              mayExist->count++;
            } else {
              addNewEdgeWithType(ET_ENTRY, currIns->block->fun->entry, currIns->block, cfgStructArg);
            }
	}

	// Update pointers
	curFunct[currIns->event.ins.tid] = currIns->block->fun;
	curBlock[currIns->event.ins.tid] = currIns->block;
        curFunct[currIns->event.ins.tid]->count = curFunct[currIns->event.ins.tid]->count + 1;
        addNewEdgeWithType(ET_CALL, prevIns->block, currIns->block->fun->first, cfgStructArg);
	return;
    }
    else if (isRet (instruc)) {
        // Look for the return address on the function stack
	uint64_t returnAddress = currIns->event.ins.addr;
	cfgStack *returnValOnFunctionStack = stack[currIns->event.ins.tid];
	while (returnValOnFunctionStack != NULL && returnValOnFunctionStack->addr != returnAddress) {
            //printf("Value not on top of stack: %llx found (cur ins is %llx)\n", returnValOnFunctionStack->addr, returnAddress );
	    returnValOnFunctionStack = returnValOnFunctionStack->next;
	}
	// See if we've set this to a ret block before
	Bbl *exitBlock = prevIns->block->fun->exit;
	if (prevIns->block->btype != BT_RET) {
            // See if the exit block already exists for this function
            if (exitBlock == NULL){
	      exitBlock = newBlock (prevIns, currIns, cfgStructArg);
	      determineBlockTypeFromEdgeType(ET_EXIT, exitBlock);
              addBlockIntoFunction(prevIns->block->fun, exitBlock);
            }
	    // At this point temp represents the block we return to
            addNewEdgeWithType(ET_EXIT, prevIns->block, exitBlock, cfgStructArg);
            determineBlockTypeFromEdgeType(ET_RET, prevIns->block); 
	} // end if (first time seeing ret block for function)
	// Otherwise this function already has an exit block
	else {
	    exitBlock = prevIns->block->fun->exit;
	    Edge *exitEdge = prevIns->block->succs;
	    Edge *exitEdgeCopy = exitBlock->preds;
	    exitEdge->count++;
	    exitEdgeCopy->count++;
	}
        // If we could not find the return address then we treat the ret as an indirect jump
        // Unless it existed in a previous phase
        if (returnValOnFunctionStack == NULL) {
            // Scan for if this ins's function appeared in a previous phase
            cfg *phaseWalk = cfgStructArg->curCfgStruct->prevPhase;
            while(phaseWalk != NULL){
              if(get_key_val(phaseWalk->htablefunc, currIns->event.ins.fnId) != NULL){
                break;
              }
              phaseWalk = phaseWalk->prevPhase;
            }
            // If its the first time we've seen a return here and we've seen it in another phase
            // then build a function for it
            if(phaseWalk != NULL && currIns->block->fun == NULL){
              // If the function appeared in another phase then we can create a new entry block for it
              // I.E. it isn't a ret-indir case
              Function *newFunct = newFunctionWithPseudoEntry(currIns, cfgStructArg);
              put_key_val(htablefunc, newFunct->id, (void *)newFunct);
              insertIntoFunctionList(cfgStructArg->curCfgStruct, newFunct);
              addBlockIntoFunction(newFunct, currIns->block);
              addNewEdgeWithType(ET_ENTRY, newFunct->entry, currIns->block, cfgStructArg);
              Edge *walk = existingEdge(exitBlock, currIns->block, NULL, NULL, cfgStructArg->disState);
              if (walk != NULL){
                walk->count++;
              } else {
                addNewEdgeWithType(ET_RET, exitBlock, newFunct->entry, cfgStructArg);
              }
              return;
            }
            // Otherwise we have the case we find in library code, i.e.
            // returns to spots we haven't been before (hence treat as indirect jump)
            Edge *walk = existingEdge(exitBlock, currIns->block, NULL, NULL, cfgStructArg->disState);
            if (walk != NULL){
              walk->count++;
            } else {
              addNewEdgeWithType(ET_INDIR, exitBlock, currIns->block, cfgStructArg);
            }
            return;
        } else { // Otherwise connect with a RET edge if one doesn't exist
            Edge *walk = existingEdge(exitBlock, currIns->block, NULL, NULL, cfgStructArg->disState);
            if (walk != NULL){
              walk->count++;
            } else {
              addNewEdgeWithType(ET_RET, exitBlock, currIns->block, cfgStructArg);
            } 
        }
        // See if we need to add the LINK edge
        Edge *walk = existingEdge(returnValOnFunctionStack->block, currIns->block, NULL, NULL, cfgStructArg->disState);
        if (walk != NULL){
          walk->count++;
        } else {
          addNewEdgeWithType(ET_LINK, returnValOnFunctionStack->block, currIns->block, cfgStructArg);
        }
	// Pop the stack up till returnValOnFunctionStack
	while (stack[currIns->event.ins.tid]->addr != returnValOnFunctionStack->addr) {
            //printf("Popping %llx looking for %llx\n", stack[currIns->event.ins.tid]->addr, returnValOnFunctionStack->addr);
	    stack[currIns->event.ins.tid] = stack[currIns->event.ins.tid]->next;
	}
        // Pop final value
        if(stack[currIns->event.ins.tid]->addr == returnValOnFunctionStack->addr){
            //printf("Popping %llx looking for %llx\n", stack[currIns->event.ins.tid]->addr, returnValOnFunctionStack->addr);
            stack[currIns->event.ins.tid] = stack[currIns->event.ins.tid]->next;
        }
	curFunct[currIns->event.ins.tid] = returnValOnFunctionStack->block->fun;
	curBlock[currIns->event.ins.tid] = currIns->block;
	curFunct[currIns->event.ins.tid]->count = curFunct[currIns->event.ins.tid]->count + 1;
    }
    // Else its a normal edge
    else {
	determineBlockTypeFromEdgeType(ET_NORMAL, prevIns->block);
	addNewEdgeWithType(ET_NORMAL, prevIns->block, currIns->block, cfgStructArg);
    }
}

edge_type peekEdgeType(cfgInstruction * prevIns, cfgInstruction * currIns, cfgState *cfgStructArg){
  uint8_t *bytes = prevIns->event.ins.binary;
  cfgInsInfo edgeInfo;
  fetchInsInfoNoReaderState(cfgStructArg->disState, prevIns, &edgeInfo);
  xed_iclass_enum_t instruc = edgeInfo.insClass;

  if (isConditionalJump(instruc)) {
    edge_type edgeType = StraightLineCode(prevIns->event.ins, currIns->event.ins) ? ET_FALSE : ET_TRUE;
    return edgeType;
  }
  else if (isUnconditionalJump(instruc)) {
    // Default unconditional edge type is JMP, unless proven to be an indirect jump
    edge_type edgeType = (bytes[0]==255) ? ET_INDIR : ET_JMP;
    return edgeType;
  }
  else if (isCall(instruc)) {
    return ET_CALL;
  }
  else if(isRet(instruc)){
    return ET_RET;
  }
  else {
    return ET_NORMAL;
  }
}

// Function name: searchThroughSuccessors(Bbl *from, Bbl *to)
// Purpose: Walk through successor edges of the "from" block, return the "to" block if it is found, NULL if it is not found
// Parameters: The block to search from and the target of the search
// Returns: The edge if it exists, null otherwise
Edge *searchThroughSuccessors(Bbl *from, Bbl *to){
  // Then while there are still successors to look at in prevBbl try to see if we have an edge already
  Edge *temp = from->succs;
  // Look for an already existing edge
  while (temp != NULL) {
    //----Existing edge between previous instruction and current block
    if (temp->to->id == to->id) {
      return temp;
    }
    temp = temp->next;
  }
  return NULL;
}

// Function name: searchThroughSuccessorsWithType(edge_type type, Bbl *from, Bbl *to)
// Purpose: Walk through successor edges of the "from" block, return the "to" block if it is found with correct edge type, NULL if it is not found
// Parameters: The block to search from and the target of the search, along with specific edge type
// Returns: The edge if it exists with given edge type, null otherwise
Edge *searchThroughSuccessorsWithType(edge_type type, Bbl *from, Bbl *to){
  // Then while there are still successors to look at in prevBbl try to see if we have an edge already
  Edge *temp = from->succs;
  // Look for an already existing edge
  while (temp != NULL) {
    //----Existing edge between previous instruction and current block
    if (temp->to->id == to->id && temp->etype == type) {
      return temp;
    }
    temp = temp->next;
  }
  return NULL;
}

Edge *searchThroughSuccessorsForDynamic(Bbl *from, Bbl *to){
  // Then while there are still successors to look at in prevBbl try to see if we have an edge already
  Edge *temp = from->succs;
  // Look for an already existing edge
  while (temp != NULL) {
    //----Existing edge between previous instruction and current block
    if (temp->to->id == to->id && temp->isDynamic == 1) {
      return temp;
    }
    temp = temp->next;
  }
  return NULL;
}

// Function: searchThroughPredecessors(Bbl *from, Bbl *to)
// Purpose: Walk through the predecessors of the "from" block and determine if the "to" block is a successor
// Parameters: The block whose predecessors edges we are searching through and the target of the search
// returns: Edge between blocks or NULL if none exists
Edge *searchThroughPredecessors(Bbl *from, Bbl *to){
  // Then while there are still successors to look at in prevBbl try to see if we have an edge already
  Edge *temp = to->preds;
  // Look for an already existing edge
  while (temp != NULL) {
    //----Existing edge between previous instruction and current block
    if (temp->from->id == from->id) {
      return temp;
    }
    temp = temp->next;
  }
  return NULL;
}

// Function name: ExistingEdge(Bbl *prevBbl, Bbl *currBbl, cfgInstruction *prevIns, cfgInstruction *currIns, DisassemblerState *disState)
// Purpose: Find if there is an existing edge between two blocks (look through successor's of block)
// Parameters: The current instruction, previous instruction. and their respective blocks
// Returns: The edge if it exists, null otherwise
Edge *
existingEdge (Bbl * prevBbl, Bbl * currBbl, cfgInstruction * prevIns, cfgInstruction * currIns, DisassemblerState *disState){
    // In the case where the blocks are not in the same function 
    // we need to compare edges from prevIns's successor to the entry/exit block.
    // Note there are times when we call this function with prevIns/currIns as NULL because we know that the edge being created
    // is within a single function (i.e. entry edge from pseudo block)
    xed_iclass_enum_t prevInsType = -1;
    if(prevIns != NULL){
      cfgInsInfo edgeInfo;
      fetchInsInfoNoReaderState(disState, prevIns, &edgeInfo);
      prevInsType = edgeInfo.insClass;
    }
    if (prevIns != NULL && currIns != NULL && (prevBbl->fun->id != currBbl->fun->id || isCall (prevInsType) || isRet (prevInsType))) {
	Bbl *cBbl = currBbl;
	Bbl *pBbl = prevBbl;
	// If we have a call as the prev Ins then we want to look at the entry block of currIns's 
	if (isCall (prevInsType)) {
	    // If the prevIns is a call then look at the edge between the entry block and the call instruction
	    cBbl = currBbl->fun->first;
	    // Since we are return the edge between the psuedoblock and the currIns, we will be skipping
	    // Through the pseudo-edge. This means we should increment the count of the pseudo-edge we are walking through
            Edge *walkedThrough = searchThroughSuccessors(cBbl, currBbl);
            if (walkedThrough != NULL){
              walkedThrough->count++;
              if(walkedThrough->cfgPhase != cBbl->fun->cfgPhase){
                addToEdgePhaseList(walkedThrough, cBbl->fun->cfgPhase->id);
                walkedThrough->cfgPhase = cBbl->fun->cfgPhase;
              }
            }
	}
	// If we have ret as the prevIns then we want to compare the edge between the exit block and the block we return to
	else if (isRet (prevInsType)) {
            // Two cases:
            // If we have a return Instruction in an unconditional block then treat like normal case
	    // If we have a exit block then set the prevBbl to be the exit block so we can look for the return edge
            if (prevBbl->btype == BT_UNCOND){
              Edge *edg = searchThroughSuccessors(prevBbl, currBbl);
              if(edg != NULL){
                if(edg->cfgPhase != currBbl->fun->cfgPhase){
                  addToEdgePhaseList(edg, currBbl->fun->cfgPhase->id);
                  edg->cfgPhase = currBbl->fun->cfgPhase;
                }
              }
              return(edg);
            }

	    // also increment the edge count between the psuedo-block and the acutal block
	    Edge *toPseudo = prevBbl->succs;
            // If we have a ret block that doesn't link to the pseudo exit then link them
            if (toPseudo == NULL){
              return NULL;
            }
            pBbl = prevBbl->fun->exit;
            if(pBbl == NULL){
              return NULL;
            }
            Edge *walkedThrough = searchThroughSuccessors(prevBbl, pBbl);
            if (walkedThrough != NULL){
              walkedThrough->count++;
              if(walkedThrough->cfgPhase != pBbl->fun->cfgPhase){
                addToEdgePhaseList(walkedThrough, pBbl->fun->cfgPhase->id);
                walkedThrough->cfgPhase = pBbl->fun->cfgPhase;
              }
            }
	}
        Edge *edg = searchThroughSuccessors(pBbl, cBbl);
        if(edg != NULL){
          if(edg->cfgPhase != cBbl->fun->cfgPhase){
            addToEdgePhaseList(edg, cBbl->fun->cfgPhase->id);
            edg->cfgPhase = cBbl->fun->cfgPhase;
          }
        }
        return(edg);
    }
    // If both blocks are in the same function, then we can look directly at their successors
    else {
        Edge *edg = searchThroughSuccessors(prevBbl, currBbl);
        if(edg != NULL){
          if(edg->cfgPhase != currBbl->fun->cfgPhase){
            addToEdgePhaseList(edg, currBbl->fun->cfgPhase->id);
            edg->cfgPhase = currBbl->fun->cfgPhase;
          }
        }
        return(edg);
    }
}

// Function name: removeEdge(Edge* remEdge);
// Purpose: remove an edge from a CFG. This is useful in merging when we encounter pseudo-blocks
// Parameters: a double pointer to the edge list containing the edge to be removed, and a pointer to the edge to be removed.
// Returns: void.
void removeEdge(Edge** edgeList, Edge* remEdge){
	if(remEdge == NULL){
		return;
	}
	
	//update head of list if necessary
	if(*edgeList == remEdge)
		*edgeList = remEdge->next;

	if(remEdge->next != NULL)	
		remEdge->next->prev = remEdge->prev;
	if(remEdge->prev != NULL)
		remEdge->prev->next = remEdge->next;

	free(remEdge);
}

// Function name: freeEdgeList(Edge* remEdge);
// Purpose: remove a list of edges. This is useful when deleting blocks to prevent memory leaks
// Parameters: a pointer to the head of the edge list to be removed.
// Returns: void.
void freeEdgeList(Edge* remEdge){
	if(remEdge == NULL){
		return;
	}

	Edge* temp = NULL;
	while(remEdge != NULL){
		temp = remEdge;
		remEdge = remEdge->next;
		free(temp);
	}
}




