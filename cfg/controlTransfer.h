/*
* File: controlTransfer.h
* Description: header file for the jump checking logic
*/

#ifndef _CONTROLTRANSFER_H_
#define _CONTROLTRANSFER_H_

#include "xed-interface.h"
#include "xed-iclass-enum.h"
#include "cfg.h"
#include "utils.h"
#include <Reader.h>

// Function Name: positionIndepCode
// Purpose: Determine whether the instruction passed in (currIns) is position independent code
//          We do this to see what type of edge we need to create
// Returns: 1(true) if the current Instruction (currIns) is position independent code, 0 otherwise
// Parameters: The previous and current instruction
// Exceptions/Errors from library calls: None
int positionIndepCode(cfgInstruction *prevIns, cfgInstruction *currIns, DisassemblerState *disState);

// Function Name: isConditionalJump
// Purpose: Determine whether the instruction passed in is a conitional jump
// Returns: 1(true) if the instruction passed in is a conditional jump instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isConditionalJump(xed_iclass_enum_t inst);

// Function Name: isPop
// Purpose: Determine whether the instruction passed in is a pop instruction
// Returns: 1(true) if the instruction passed in is a pop instruction, 0 otherwise
// Parameters: the cfgInstruction to examine
// Exceptions/Errors from library calls: None
int isPop(cfgInstruction *Ins, DisassemblerState *disState);

// Function Name: isUnconditionalJump
// Purpose: Determine whether the instruction passed in is an unconditional jump instruction
// Returns: 1(true) if the instruction passed in is an unconditional jump instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isUnconditionalJump(xed_iclass_enum_t instruc);

// Function Name: isCall
// Purpose: Determine whether the instruction passed in is a call instruction
// Returns: 1(true) if the instruction passed in is a call instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isCall(xed_iclass_enum_t instruc);

// Function Name: isRet
// Purpose: Determine whether the instruction passed in is a return instruction
// Returns: 1(true) if the instruction passed in is a return instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isRet(xed_iclass_enum_t instruc);

// Function Name: isControlTransfer
// Purpose: Determine whether the instruction passed in is a control transfer function (ex is it a jump, ret, call, pop, etc)
// Returns: 1(true) if the instruction passed in is a control transfer instruction, 0 otherwise
// Parameters: The instruction which may or may not be a control transfer instruction
// Exceptions/Errors from library calls: None
int isControlTransfer(cfgInstruction *inst, DisassemblerState *disState);

#endif
