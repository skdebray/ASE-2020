
#include <assert.h>
#include "BytePropagation.h"
#include "ByteRegTaint.h"
#include "ByteMemoryTaint.h"
#include "LabelStore.h"
#include "TaintState.h"
#include "Taint.h"
#include <Reader.h>

/*******************************************************************************
 *                                                                             *
 *      Instructions and registers handled specially for taint propagation     *
 *                                                                             *
 *******************************************************************************/

uint8_t isPushIns(xed_iclass_enum_t inst){
  uint8_t res = (inst == XED_ICLASS_PUSH
		 || inst == XED_ICLASS_PUSHA
		 || inst == XED_ICLASS_PUSHAD
		 || inst == XED_ICLASS_PUSHF
		 || inst == XED_ICLASS_PUSHFD
		 || inst == XED_ICLASS_PUSHFQ);
  return res;
}

uint8_t isPopIns(xed_iclass_enum_t inst){
  uint8_t res = (inst == XED_ICLASS_POP
		 || inst == XED_ICLASS_POPA
		 || inst == XED_ICLASS_POPAD
		 || inst == XED_ICLASS_POPF
		 || inst == XED_ICLASS_POPFD
		 || inst == XED_ICLASS_POPFQ
		 || inst == XED_ICLASS_POPCNT);
  return res;
}

uint8_t canSkipTaintBecauseInsType(xed_iclass_enum_t inst){
  uint8_t isRet = (inst == XED_ICLASS_RET_FAR
		   || inst == XED_ICLASS_RET_NEAR
		   || inst == XED_ICLASS_IRET
		   || inst == XED_ICLASS_IRETD
		   || inst == XED_ICLASS_IRETQ
		   || inst == XED_ICLASS_SYSRET
		   || inst == XED_ICLASS_SYSRET_AMD);
  uint8_t isCall = (inst == XED_ICLASS_CALL_FAR
		    || inst == XED_ICLASS_CALL_NEAR
		    || inst == XED_ICLASS_SYSCALL
		    || inst == XED_ICLASS_SYSCALL_AMD);
  uint8_t isLeave = (inst == XED_ICLASS_LEAVE);
  uint8_t isEnter = (inst == XED_ICLASS_ENTER);
  return(isRet || isCall || isLeave || isEnter || isPushIns(inst) || isPopIns(inst));
}

/*******************************************************************************
 *                                                                             *
 *                                TAINT FETCHING                               *
 *                                                                             *
 *******************************************************************************/

/* 
 * getAddrCalcTaint() -- given a memory addressing operand op and a taint 
 * label curLabel, this function returns the taint label resulting from 
 * combining curLabel with any taint on any of the registers used by op.
 */
uint64_t getAddrCalcTaint(TaintState *state, uint32_t tid, ReaderOp *op, uint64_t curLabel) {
    if(op->mem.seg != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint,
					   state->labelState,
					   op->mem.seg, tid,
					   curLabel);
    }

    if(op->mem.base != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint,
					   state->labelState,
					   op->mem.base,
					   tid,
					   curLabel);
    }

    if(op->mem.index != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint,
					   state->labelState,
					   op->mem.index,
					   tid,
					   curLabel);
    }

    return curLabel;
}

uint64_t getOpTaint(TaintState *state,
		    uint32_t tid,
		    ReaderOp *op,
		    uint64_t curLabel,
		    uint8_t keepRBP) {
    if(op->type == REG_OP) {
        if(op->reg != LYNX_GFLAGS) {
            curLabel = getCombinedByteRegTaint(state->regTaint,
					       state->labelState,
					       op->reg,
					       tid,
					       curLabel);
        }
    }
    else if(op->type == MEM_OP) {
        curLabel = getCombinedByteMemTaint(state->memTaint,
					   state->labelState,
					   op->mem.addr,
					   op->mem.size,
					   curLabel);

        if(op->mem.seg != LYNX_INVALID && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint,
					       state->labelState,
					       op->mem.seg,
					       tid,
					       curLabel);
        }

        if(op->mem.base != LYNX_INVALID
	   && op->mem.base != LYNX_RIP
	   && op->mem.base != LYNX_RSP
	   && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint,
					       state->labelState,
					       op->mem.base,
					       tid,
					       curLabel);
        }

        if(op->mem.index != LYNX_INVALID
	   && op->mem.base != LYNX_RIP
	   && op->mem.base != LYNX_RSP
	   && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint,
					       state->labelState,
					       op->mem.index,
					       tid,
					       curLabel);
        }
    }

    return curLabel;
}


/*
 * getAddrCalcListTaint() -- given a taint label curLabel and a collection of
 * memory operands ops:
 * -- if keepRegInAddrCalc != 0: return the taint label obtained by combining 
 *    curLabel  with the taint in any of the registers involved in address 
 *    calculations in ops.  
 * -- if keepRegInAddrCalc == zero: ignore taint from registers involved in
 *    address calculations and return the value of curLabel.
 */
uint64_t getAddrCalcListTaint(TaintState *state,
			      uint32_t tid,
			      ReaderOp *ops,
			      uint32_t cnt,
			      uint64_t curLabel,
			      uint8_t keepRegInAddrCalc) {
    if(!keepRegInAddrCalc){
      return curLabel;
    }
    int i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == MEM_OP) {
            curLabel = getAddrCalcTaint(state, tid, ops, curLabel);
        }
        ops = ops->next;
    }

    return curLabel;
}

uint64_t getOpListTaint(TaintState *state,
			uint32_t tid,
			ReaderOp *ops,
			uint32_t cnt,
			uint64_t curLabel,
			uint8_t keepReg,
			xed_iclass_enum_t inst) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        uint8_t keepRBP = 1;
        if(!keepReg && canSkipTaintBecauseInsType(inst)){
          if(ops->type == REG_OP){
            LynxReg parent = LynxReg2FullLynxReg(ops->reg);
            if(parent == LYNX_RSP
	       || parent == LYNX_RIP
	       || (parent == LYNX_RBP
		   && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
              ops = ops->next;
              continue;
            }
          } else if(ops->type == MEM_OP
		    && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER)){
            keepRBP = 0;
          }
        }
        curLabel = getOpTaint(state, tid, ops, curLabel, keepRBP);
        ops = ops->next;
    }
    return curLabel;
}

/*
 * getTopLevelListTaint() -- given a collection of instruction operands ops
 * and a taint label curLabel, return the taint obtained by combining
 * curLabel with the taint in all of the (non-GFLAGS) operands in ops.
 */
uint64_t getTopLevelListTaint(TaintState *state,
			      uint32_t tid,
			      ReaderOp *ops,
			      uint32_t cnt,
			      uint64_t curLabel) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == REG_OP) {
            if(ops->reg != LYNX_GFLAGS && ops->reg != LYNX_RIP && ops->reg != LYNX_RSP) {
                curLabel = getCombinedByteRegTaint(state->regTaint,
						   state->labelState,
						   ops->reg,
						   tid,
						   curLabel);
            }
        }
        else if(ops->type == MEM_OP) {
            curLabel = getCombinedByteMemTaint(state->memTaint,
					       state->labelState,
					       ops->mem.addr,
					       ops->mem.size,
					       curLabel);
        }
        ops = ops->next;
    }

    return curLabel;
}

/*******************************************************************************
 *                                                                             *
 *                                TAINT SETTING                                *
 *                                                                             *
 *******************************************************************************/

/*
 * setAllMemTaint() -- set the taint label for size memory locations starting 
 * at addr to label.
 */
void setAllMemTaint(TaintState *state, uint64_t addr, uint64_t size, uint64_t label) {
    if(size != 0) {
        setAllByteMemTaint(state->memTaint, addr, size, label);
    }
}

/*
 * setAllRegTaint() -- set the taint label for all of the bytes of register reg 
 * (provided that reg is not one of {RIP, RSP, GFLAGS}) to label.
 */
void setAllRegTaint(TaintState *state, uint32_t tid, LynxReg reg, uint64_t label) {
    LynxReg fullReg = getFullReg(state->readerState, reg);

    if(fullReg == LYNX_RIP || fullReg == LYNX_RSP || fullReg == LYNX_GFLAGS) {
        return;
    }

    setAllByteRegTaint(state->regTaint, reg, tid, label);
}

/*
 * setAllOpListTaint() -- Set the taint label for all of the operands in ops 
 * to the label combined.  Depending on the kind of instruction inst under
 * consideration, and the flag keepReg, we may cut specific registers
 * such as RSP, RIP, and RBP some slack.
 */
void setAllOpListTaint(TaintState *state,
		       uint32_t tid,
		       ReaderOp *ops,
		       uint32_t cnt,
		       uint64_t combined,
		       uint8_t keepReg,
		       xed_iclass_enum_t inst) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == REG_OP) {
            if(!keepReg && canSkipTaintBecauseInsType(inst)){
              if(ops->type == REG_OP){
                LynxReg parent = LynxReg2FullLynxReg(ops->reg);
                if(parent == LYNX_RSP
		   || parent == LYNX_RIP
		   || (parent == LYNX_RBP
		       && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
                  if((parent == LYNX_RBP
		      && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
                  }
                  ops = ops->next;
                  continue;
                }
              }
            }
            int size = getRegSize(state->readerState, ops->reg);
            if(size == 4) {
                setAllRegTaint(state, tid, getFullReg(state->readerState, ops->reg), 0);
            }

            setAllRegTaint(state, tid, ops->reg, combined);

        }
        else if(ops->type == MEM_OP) {
            setAllMemTaint(state, ops->mem.addr, ops->mem.size, combined);
        }
        ops = ops->next;
    }
}


/*
 * setAddrCalcListTaint() -- given a set of operands ops, add label to the
 * taint label of all of the registers involved in address calculations in 
 * the set of operands ops.
 */
void setAddrCalcListTaint(TaintState *state,
			  uint32_t tid,
			  ReaderOp *ops,
			  uint32_t cnt,
			  uint64_t label) {
    int i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == MEM_OP) {
            if(ops->mem.seg != LYNX_INVALID) {
                setAllRegTaint(state, tid, ops->mem.seg, label);
            }

            if(ops->mem.base != LYNX_INVALID) {
                setAllRegTaint(state, tid, ops->mem.base, label);
            }

            if(ops->mem.index != LYNX_INVALID) {
                setAllRegTaint(state, tid, ops->mem.index, label);
            }
        }
        ops = ops->next;
    }
}

/*
 * combineAddrCalcTaint() -- for each register used for address calculations
 * in operand op, set its taint to the result of combining it with the taint
 * label combined.
 */
void combineAddrCalcTaint(TaintState *state,
			  uint32_t tid,
			  ReaderOp *op,
			  uint64_t combined) {
    if(op->mem.seg != LYNX_INVALID) {
        addTaintToByteReg(state->regTaint,
			  state->labelState,
			  op->mem.seg,
			  tid,
			  combined);
    }

    if(op->mem.base != LYNX_INVALID
       && op->mem.base != LYNX_RIP
       && op->mem.base != LYNX_RSP) {
        addTaintToByteReg(state->regTaint,
			  state->labelState,
			  op->mem.base,
			  tid,
			  combined);
    }

    if(op->mem.index != LYNX_INVALID
       && op->mem.index != LYNX_RIP
       && op->mem.index != LYNX_RSP) {
        addTaintToByteReg(state->regTaint,
			  state->labelState,
			  op->mem.index,
			  tid,
			  combined);
    }
}


/*
 * combineAddrCalcListTaint() -- given a set of operands ops, add the taint
 * label apply to the taint label for each register that is used for any
 * address calculations in ops.
 */
void combineAddrCalcListTaint(TaintState *state,
			      uint32_t tid,
			      ReaderOp *ops,
			      uint32_t cnt,
			      uint64_t apply) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == MEM_OP) {
            combineAddrCalcTaint(state, tid, ops, apply);
        }
        ops = ops->next;
    }
}


/*
 * combineOpListTaint() -- add the taint label combined to each operand in
 * the set of operands ops, including registers used for address computations
 * in ops but excluding RIP, RSP, and GFLAGS.
 */
void combineOpListTaint(TaintState *state,
			uint32_t tid,
			ReaderOp *ops,
			uint32_t cnt,
			uint64_t combined) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == REG_OP) {
            LynxReg fullReg = getFullReg(state->readerState, ops->reg);

            if(fullReg == LYNX_RIP || fullReg == LYNX_RSP || fullReg == LYNX_GFLAGS) {
                return;
            }

            addTaintToByteReg(state->regTaint, state->labelState, ops->reg, tid, combined);
        }
        else if(ops->type == MEM_OP) {
            uint64_t addr = ops->mem.addr;
            uint32_t size = ops->mem.size;

            addTaintToByteMem(state->memTaint, state->labelState, addr, size, combined);

            combineAddrCalcTaint(state, tid, ops, combined);
        }
        ops = ops->next;
    }
}


/*******************************************************************************
 *                                                                             *
 *                          FORWARD TAINT PROPAGATION                          *
 *                                                                             *
 *******************************************************************************/

/*
 * getSrcTaint() -- collect the taint from all of the inputs to an instruction.  
 * These are:
 *
 *    -- flags used by the instruction;
 *    -- source operands and read-write operands; and
 *    -- registers used for address computations for destination registers.
 *
 * If the argument keepRegInAddrCalc is zero, taint propagation from registers
 * RSP and RIP (and sometimes RBP) is omitted for instructions, such as push, 
 * pop, call, ret, enter, leave, etc., that manipulate the stack.
 */
uint64_t getSrcTaint(TaintState *state,
		     ReaderEvent *event,
		     InsInfo *info,
		     uint64_t curLabel,
		     uint8_t keepRegInAddrCalc) {
    /*
     * If the instruction uses any flags, propagate any taint from the flags
     */
    if(info->srcFlags != 0) {
        curLabel = getByteFlagTaint(state->regTaint,
				    state->labelState,
				    event->ins.tid,
				    info->srcFlags,
				    curLabel);
    }

    /*
     * Propagate any taint from any of the bytes occupied by the instruction
     * (this corresponds to a codegen dependency)
     */
    curLabel = getCombinedByteMemTaint(state->memTaint,
				       state->labelState,
				       event->ins.addr,
				       event->ins.binSize,
				       curLabel);

    /*
     * Propagate taint from the source operands and read-write operands
     */
    curLabel = getOpListTaint(state,
			      event->ins.tid,
			      info->srcOps,
			      info->srcOpCnt,
			      curLabel,
			      keepRegInAddrCalc,
			      info->insClass);
    curLabel = getOpListTaint(state,
			      event->ins.tid,
			      info->readWriteOps,
			      info->readWriteOpCnt,
			      curLabel,
			      keepRegInAddrCalc,
			      info->insClass);
    /*
     * Propagate taint from registers used for address computations for 
     * destination operands and return the resulting accumulated source taint.
     */
    return getAddrCalcListTaint(state,
				event->ins.tid,
				info->dstOps,
				info->dstOpCnt,
				curLabel,
				keepRegInAddrCalc);
}


/*
 * defaultBytePropagation() -- processes a single event.  It collects all of
 * the input taint for the event and propagates this to all of the outputs
 * of the event (dst and read-write operators as well as dst flags).  The value
 * returned is the taint label so propagated.
 */
uint64_t defaultBytePropagation(TaintState *state,
				ReaderEvent *event,
				InsInfo *info,
				uint8_t keepRegInAddrCalc) {
    uint64_t label = 0;
    label = getSrcTaint(state, event, info, label, keepRegInAddrCalc);

    setByteFlagTaint(state->regTaint,
		     state->labelState,
		     event->ins.tid,
		     info->dstFlags, label);
    setAllOpListTaint(state,
		      event->ins.tid,
		      info->readWriteOps,
		      info->readWriteOpCnt,
		      label,
		      keepRegInAddrCalc,
		      info->insClass);
    setAllOpListTaint(state,
		      event->ins.tid,
		      info->dstOps,
		      info->dstOpCnt,
		      label,
		      keepRegInAddrCalc,
		      info->insClass);

    return label;
}


/*
 * xorBytePropagation() -- handles taint propagation for the special case where
 * a location (reg or mem) is XOR'd with itself, resulting in a zero value that
 * is untainted irrespective of the taint on the locations that are XOR'd.
 */
uint64_t xorBytePropagation(TaintState *state,
			    ReaderEvent *event,
			    InsInfo *info,
			    uint8_t keepRegInAddrCalc) {
    assert((info->readWriteOpCnt + info->srcOpCnt) == 2);
    ReaderOp *op1 = info->srcOps;
    ReaderOp *op2;

    if(info->srcOpCnt == 2) {
        op2 = info->srcOps->next;
    }
    else {
        op2 = info->readWriteOps;
    }

    if(op1->type == REG_OP && op2->type == REG_OP) {
        if(op1->reg == op2->reg) {
            setAllOpListTaint(state,
			      event->ins.tid,
			      info->readWriteOps,
			      info->readWriteOpCnt,
			      0,
			      keepRegInAddrCalc,
			      info->insClass);
            setAllOpListTaint(state,
			      event->ins.tid,
			      info->dstOps,
			      info->dstOpCnt,
			      0,
			      keepRegInAddrCalc,
			      info->insClass);
            return 0;
        }
    }
    else if(op1->type == MEM_OP && op2->type == REG_OP) {
        if(op1->mem.addr == op2->mem.addr && op1->mem.size == op2->mem.size) {
            setAllOpListTaint(state,
			      event->ins.tid,
			      info->readWriteOps,
			      info->readWriteOpCnt,
			      0,
			      keepRegInAddrCalc,
			      info->insClass);
            setAllOpListTaint(state,
			      event->ins.tid,
			      info->dstOps,
			      info->dstOpCnt,
			      0,
			      keepRegInAddrCalc,
			      info->insClass);
            return 0;
        }
    }

    return defaultBytePropagation(state, event, info, keepRegInAddrCalc);
}


/*
 * initByteHandlers() -- initialize handlers for different instruction classes.
 * Currently we use default taint propagation for all cases except for the case
 * where a location is XOR'd with itself.
 */
void initByteHandlers(uint64_t (**handlers) (TaintState *state,
					     ReaderEvent *event,
					     InsInfo *info,
					     uint8_t keepRegInAddrCalc)) {
    int i;
    for(i = 0; i < XED_ICLASS_LAST; i++) {
        handlers[i] = defaultBytePropagation; 
    }   

    handlers[XED_ICLASS_KXORB] = xorBytePropagation;
    handlers[XED_ICLASS_KXORD] = xorBytePropagation;
    handlers[XED_ICLASS_KXORQ] = xorBytePropagation;
    handlers[XED_ICLASS_KXORW] = xorBytePropagation;
    handlers[XED_ICLASS_PXOR] = xorBytePropagation;
    handlers[XED_ICLASS_VPXOR] = xorBytePropagation;
    handlers[XED_ICLASS_VPXORD] = xorBytePropagation;
    handlers[XED_ICLASS_VPXORQ] = xorBytePropagation;
    handlers[XED_ICLASS_VXORPD] = xorBytePropagation;
    handlers[XED_ICLASS_VXORPS] = xorBytePropagation;
    handlers[XED_ICLASS_XOR] = xorBytePropagation;
    handlers[XED_ICLASS_XORPD] = xorBytePropagation;
    handlers[XED_ICLASS_XORPS] = xorBytePropagation;
}


/*******************************************************************************
 *                                                                             *
 *                          BACKWARD TAINT PROPAGATION                         *
 *                                                                             *
 *******************************************************************************/

/*
 * getDstTaint() -- collect the taint from all outputs of an instruction, namely:
 *
 *    -- flags written by the instruction;
 *    -- destination operands;
 *    -- read-write operands.
 */
uint64_t getDstTaint(TaintState *state,
		     uint32_t tid,
		     InsInfo *info,
		     uint64_t curLabel) {
    /*
     * Collect into label all of the taint in the destination operands and flags
     */
    if(info->dstFlags != 0) {
        curLabel = getByteFlagTaint(state->regTaint,
				    state->labelState,
				    tid,
				    info->dstFlags,
				    curLabel);
    }
    
    curLabel = getTopLevelListTaint(state, tid, info->dstOps, info->dstOpCnt, curLabel);

    /*
     * Combine the resulting taint with the taint in the read-write operands
     * and return the resulting taint.
     */
   return getTopLevelListTaint(state,
			       tid,
			       info->readWriteOps,
			       info->readWriteOpCnt,
			       curLabel);
}


/*
 * backwardBytePropagation() -- propagate taint backwards from dst to src operands
 */
uint64_t backwardBytePropagation(TaintState *state, uint32_t tid, InsInfo *info) {
    uint64_t label = 0;

    label = getDstTaint(state, tid, info, label);

    if(label) {
        setByteFlagTaint(state->regTaint, state->labelState, tid, info->dstFlags, 0);
        setAllOpListTaint(state, tid, info->dstOps, info->dstOpCnt, 0, 0, info->insClass);

        if(info->insClass == XED_ICLASS_XOR || info->insClass == XED_ICLASS_PXOR) {
            ReaderOp *op1 = info->srcOps;
            ReaderOp *op2;

            if(info->srcOpCnt == 2) {
                op2 = info->srcOps->next;
            }
            else {
                op2 = info->readWriteOps;
            }

            if(op1->type == REG_OP && op2->type == REG_OP) {
                if(op1->reg == op2->reg) {
                    setAllOpListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, 0, 0, info->insClass);
                    return 0;
                }
            }
            else if(op1->type == MEM_OP && op2->type == MEM_OP) {
                if(op1->mem.addr == op2->mem.addr && op1->mem.size == op2->mem.size) {
                    setAllOpListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, 0, 0, info->insClass);
                    return 0;
                }
            }
        }

        combineByteFlagTaint(state->regTaint, state->labelState, tid, info->srcFlags, label);
        setAllOpListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, label, 0, info->insClass);
        combineAddrCalcListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, label);
        combineOpListTaint(state, tid, info->srcOps, info->srcOpCnt, label);
        combineAddrCalcListTaint(state, tid, info->dstOps, info->dstOpCnt, label);

        return label;
    }

    return 0;
}


/*
 * backwardBytePropagationWithIns() -- propagate taint backwards similarly
 * to backwardBytePropagation(), but additionally taint the bytes occupied
 * by instruction.  The tainting of instruction bytes aims to deal with 
 * self-modifying code.
 */
uint64_t backwardBytePropagationWithIns(TaintState *state,
					uint32_t tid,
					InsInfo *info,
					ReaderIns instruction) {
    uint64_t label = 0;

    label = getDstTaint(state, tid, info, label);

    if(label) {
        setByteFlagTaint(state->regTaint, state->labelState, tid, info->dstFlags, 0);
        setAllOpListTaint(state, tid, info->dstOps, info->dstOpCnt, 0, 0, info->insClass);

	/*
	 * An XOR with identical operands is a special case that stops the backward 
	 * flow of taint just as it stops forward taint flow.  The idea here is that
	 * since the result computed by such an instruction is identically 0, it does
	 * not make sense to propagate taint from the destination to the source ops.
	 */
        if(info->insClass == XED_ICLASS_XOR || info->insClass == XED_ICLASS_PXOR) {
            ReaderOp *op1 = info->srcOps;
            ReaderOp *op2;

            if(info->srcOpCnt == 2) {
                op2 = info->srcOps->next;
            }
            else {
                op2 = info->readWriteOps;
            }

            if(op1->type == REG_OP && op2->type == REG_OP) {
                if(op1->reg == op2->reg) {
                    setAllOpListTaint(state,
				      tid,
				      info->readWriteOps,
				      info->readWriteOpCnt,
				      0,
				      0,
				      info->insClass);
                    return 0;
                }
            }
            else if(op1->type == MEM_OP && op2->type == MEM_OP) {
                if(op1->mem.addr == op2->mem.addr && op1->mem.size == op2->mem.size) {
                    setAllOpListTaint(state,
				      tid,
				      info->readWriteOps,
				      info->readWriteOpCnt,
				      0,
				      0,
				      info->insClass);
                    return 0;
                }
            }
        }

        combineByteFlagTaint(state->regTaint,
			     state->labelState,
			     tid,
			     info->srcFlags,
			     label);
        setAllOpListTaint(state,
			  tid,
			  info->readWriteOps,
			  info->readWriteOpCnt,
			  label,
			  0,
			  info->insClass);
	// Taint memory region of instruction
	taintMem(state, instruction.addr, instruction.binSize, label);
        combineAddrCalcListTaint(state,
				 tid,
				 info->readWriteOps,
				 info->readWriteOpCnt,
				 label);
        combineOpListTaint(state, tid, info->srcOps, info->srcOpCnt, label);
        combineAddrCalcListTaint(state, tid, info->dstOps, info->dstOpCnt, label);

        return label;
    }

    return 0;
}
