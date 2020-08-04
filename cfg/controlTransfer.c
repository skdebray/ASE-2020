/*
* File: controlTransfer.c
* Description: Code for determining if an instruction is one capable of control transfer (ex jump or call)
*              and if so what type of control transfer is it
*/
#include <xed-interface.h>
#include <xed-iclass-enum.h>
#include "cfg.h"
#include <Reader.h>
#include "utils.h"
#include "controlTransfer.h"

// Statically declared IndInfo holder (will be overrided when necessary, no need to save this state)
cfgInsInfo infoHolder;

// Function Name: positionIndepCode
// Purpose: Determine whether the instruction passed in (currIns) is position independent code
//          We do this to see what type of edge we need to create
// Returns: 1(true) if the current Instruction (currIns) is position independent code, 0 otherwise
// Parameters: The previous and current instruction
// Exceptions/Errors from library calls: None
int positionIndepCode(cfgInstruction *prevIns, cfgInstruction *currIns, DisassemblerState *disState){
  // If the next instruction is a pop instruction
  if (isPop(currIns, disState)){
    // If the next instruction is the target of the call (prevIns)
    // If the address of the next instruction is the instruction immediately following the call
    // instruction, then the next instruction was the target
    if (currIns->event.ins.addr == prevIns->event.ins.addr + prevIns->event.ins.binSize){
      // then we have position independent code
      return(1);
    } 
  }
  // Otherwise code is not pic
  return(0);
}

// Function Name: isConditionalJump
// Purpose: Determine whether the instruction passed in is a conitional jump
// Returns: 1(true) if the instruction passed in is a conditional jump instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isConditionalJump(xed_iclass_enum_t inst){
  // If the instruction is a conditional jump (of which there are many), return 1
  switch(inst) {
    case(XED_ICLASS_JO ):
    case(XED_ICLASS_JNO):
    case(XED_ICLASS_JS):
    case(XED_ICLASS_JNS):
    case(XED_ICLASS_JZ):
    case(XED_ICLASS_JNZ):
    case(XED_ICLASS_JB):
    case(XED_ICLASS_JNB):
    case(XED_ICLASS_JBE):
    case(XED_ICLASS_JNBE):
    case(XED_ICLASS_JL):
    case(XED_ICLASS_JNL):
    case(XED_ICLASS_JLE):
    case(XED_ICLASS_JNLE):
    case(XED_ICLASS_JP):
    case(XED_ICLASS_JNP):
    case(XED_ICLASS_JCXZ):
    case(XED_ICLASS_JECXZ):
      return(1);
    // Otherwise the instruction is not a conditional jump, return 0
    default:
      return(0);
  }
}

// Function Name: isPop
// Purpose: Determine whether the instruction passed in is a pop instruction
// Returns: 1(true) if the instruction passed in is a pop instruction, 0 otherwise
// Parameters: the cfgInstruction to examine
// Exceptions/Errors from library calls: None
int isPop(cfgInstruction *Ins, DisassemblerState *disState){
  // Get the XED enum for instruction type
  fetchInsInfoNoReaderState(disState, Ins, &infoHolder);
  xed_iclass_enum_t nextIns = infoHolder.insClass;
  switch(nextIns){
    // If the instruction is a pop, return 1
    case(XED_ICLASS_POP):
    case(XED_ICLASS_POPA):
    case(XED_ICLASS_POPAD):
    case(XED_ICLASS_POPCNT):
    case(XED_ICLASS_POPF):
    case(XED_ICLASS_POPFD):
    case(XED_ICLASS_POPFQ):
      return(1);
    // Otherwise it is not a pop, return 0
    default:
      return(0);
  }
}

// Function Name: isUnconditionalJump
// Purpose: Determine whether the instruction passed in is an unconditional jump instruction
// Returns: 1(true) if the instruction passed in is an unconditional jump instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isUnconditionalJump(xed_iclass_enum_t instruc){
  // If its an unconditional jump, return 1
  return (instruc == XED_ICLASS_JMP || instruc == XED_ICLASS_JMP_FAR);
}

// Function Name: isCall
// Purpose: Determine whether the instruction passed in is a call instruction
// Returns: 1(true) if the instruction passed in is a call instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isCall(xed_iclass_enum_t instruc){
  // If the instruction is a call instruction, return 1
  return (instruc == XED_ICLASS_CALL_NEAR || instruc == XED_ICLASS_CALL_FAR);
}

// Function Name: isRet
// Purpose: Determine whether the instruction passed in is a return instruction
// Returns: 1(true) if the instruction passed in is a return instruction, 0 otherwise
// Parameters: The XED class enum for the instruction type (ex jmp or ret)
// Exceptions/Errors from library calls: None
int isRet(xed_iclass_enum_t instruc){
  // If the instruction is a return Instruction, return 1
  return (instruc == XED_ICLASS_RET_NEAR || instruc == XED_ICLASS_RET_FAR);
}

// Function Name: isControlTransfer
// Purpose: Determine whether the instruction passed in is a control transfer function (ex is it a jump, ret, call, pop, etc)
// Returns: 1(true) if the instruction passed in is a control transfer instruction, 0 otherwise
// Parameters: The instruction which may or may not be a control transfer instruction
// Exceptions/Errors from library calls: None
int isControlTransfer(cfgInstruction *inst, DisassemblerState *disState){
  // Get the XED enum to determine what type of instruction we are working with
  fetchInsInfoNoReaderState(disState, inst, &infoHolder);
  xed_iclass_enum_t instruc = infoHolder.insClass;  
  // Conditional Jump
  return (isConditionalJump(instruc) || isUnconditionalJump(instruc) || isCall(instruc) || isRet(instruc));
}
