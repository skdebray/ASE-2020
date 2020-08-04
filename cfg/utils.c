/*
 * File: utils.c
 * Purpose: various utility routines relating to control flow graph construction.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "XedDisassembler.h"
#include "utils.h"
#include "cfgState.h"

/**
 * Function: fetchInsInfo
 * Description: Fetches additional information about an instruction from XED.
 * Returns: None
 **/
void fetchInsInfoNoReaderState(DisassemblerState *disState, cfgInstruction *cfgIns, cfgInsInfo *info) {
    //reset counts, but note the linked list does NOT get modified, that way we can take advantage of 
    // past allocations
    ReaderIns *ins = &(cfgIns->event.ins);
    xed_decoded_inst_t xedIns;
    decodeIns(disState, &xedIns, ins->binary, 15);

    info->insClass = getInsClass(&xedIns);
    getInsMnemonic(&xedIns, info->mnemonic, 128);
    // Fetch immediates if we haven't done so already
    if(cfgIns->fetched == 0){
      cfgIns->fetched = 1;
      //get integer operands
      const xed_inst_t *xedConstIns = xed_decoded_inst_inst(&xedIns);
      uint32_t numOps = xed_inst_noperands(xedConstIns);

      uint32_t i;
      for(i = 0; i < numOps; i++) {
        const xed_operand_t *xedOp = xed_inst_operand(xedConstIns, i);
        xed_operand_enum_t opType = xed_operand_name(xedOp);
      
        switch(opType) {
          case XED_OPERAND_IMM0:
            if (xed_decoded_inst_get_immediate_is_signed(&xedIns)) {
              cfgIns->type = SIGNED_IMM_OP;
              cfgIns->signedImm = xed_decoded_inst_get_signed_immediate(&xedIns);
            }
            else {
              cfgIns->type = UNSIGNED_IMM_OP;
              cfgIns->unsignedImm = xed_decoded_inst_get_unsigned_immediate(&xedIns);
            }
            break;
          case XED_OPERAND_IMM1:
            cfgIns->type = UNSIGNED_IMM_OP;
            cfgIns->unsignedImm = xed_decoded_inst_get_second_immediate(&xedIns);
            break;
	  default:
	    break; 
        }
      }
    }
}

/*
 * report an error message on stderr
 */
void errmsg(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);
}

/*
 * alloc(n) -- allocate a memory block of n bytes and return a pointer to the block.
 */
void *alloc(int n) {
  void *ptr = malloc(n);
  
  if (ptr == NULL) {
    errmsg("Out of memory!");
    exit(1);
  }
  
  return ptr;
}



