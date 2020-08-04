/*
* File: block.h
* Author: Jesse Bartels
* Description: Header file for all block related functions
*
*/
#ifndef _BLOCK_H_
#define _BLOCK_H_

#include "cfg.h"
#include "Reader.h"

// Function Name: newBlock
// Purpose: Create a new "default" block, allocate space, and return it
// Returns: Pointer to a new block (if alloc succceeds)
// Exceptions/Errors from library calls: Alloc (from utils.c) can throw an error if there is no more memory
Bbl* newBlock(cfgInstruction *prevIns, cfgInstruction *currIns, cfgState *cfgStructArg);

void addToBlockPhaseList(Bbl *blockArg, int phaseID);

int bblPartOfPhase(Bbl *blockArg, int phaseID);

void addInsIntoBlock(Bbl *blockToInsertInto, cfgInstruction *currIns);

void determineBlockTypeFromEdgeType(edge_type etype, Bbl *block);

// Function Name: splitBlock
// Purpose: Function that takes a block and splits it into two separate blocks, linking together the neccessary lists
// Returns: Void
// Exceptions/errors: Alloc can throw an error if there is no space left to use for allocation
void splitBlock(cfgInstruction *prevIns, cfgInstruction *currIns, Bbl *prevBbl, Bbl *currBbl, cfgState *cfgStructArg);

// Function Name: freeBlock
// Purpose: Free the blockand the list of instructions in it
// Exceptions/Errors: None
// Arguments: Pointer to the block to be freed
void freeBlock(Bbl *block);
#endif
