/*
* File: newstructfactory.h
* Author: Jesse Bartels
* Description: Header file to define all function related methods in the cfg tool
*
*/
#ifndef _FUNCTION_H_
#define _FUNCTION_H_

#include "cfg.h"
#include "Reader.h"

// Function Name: newFunction(cfgInstruction * currIns)
// Purpose: Create functions (reduce amount of duplicated code)
// Returns: Pointer to a new function if malloc is successful
// Parameters: The current and previous instruction
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory
Function* newFunction(cfgInstruction *currIns, cfgState *cfgStructArg);

// Function Name: newFunctionWithPseudoEntry(cfgInstruction * currIns)
// Purpose: Create functions with a pseudo entry block
// Returns: Pointer to a new function if malloc is successful
// Parameters: The current and previous instruction
// Exceptions/Errors from library calls: Alloc can throw an error if there is no more memory
Function* newFunctionWithPseudoEntry(cfgInstruction *currIns, cfgState *cfgStructArg);

void addToPhaseList(Function *func, int phaseID);

int funPartOfPhase(Function *func, int phaseID);

void mergeFunctions(Function *merge, Function *mergeInto, Function *curFunc, cfgState *cfgStructArg);

void addBlockIntoFunction(Function *fun, Bbl *block);

void insertIntoFunctionList(cfg *cfg, Function *fun);

void pushOntoFunctionStack(cfgInstruction *prevIns, cfgInstruction *currIns, cfgState *cfgStructArg);

// Function Name: freeFunction(Function *fun)
// Purpose: Free a function, along with all the blocks associated with the function
// Parameters: Function struct to free
// Returns: Nothing
// Exceptions/Errors: None
void freeFunction(Function *fun);
#endif
