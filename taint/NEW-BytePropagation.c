
#include <assert.h>
#include "BytePropagation.h"
#include "ByteRegTaint.h"
#include "ByteMemoryTaint.h"
#include "LabelStore.h"
#include "TaintState.h"
#include "Taint.h"
#include <Reader.h>

/**
 * Taint Fetching
 **/

uint64_t getAddrCalcTaint(TaintState *state, uint32_t tid, ReaderOp *op, uint64_t curLabel) {
    if(op->mem.seg != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.seg, tid, curLabel);
        //LynxReg parent = LynxReg2FullLynxReg(op->mem.seg);
        //printf("Get Reg:%s\n", LynxReg2Str(parent));
    }

    if(op->mem.base != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.base, tid, curLabel);
        //LynxReg parent = LynxReg2FullLynxReg(op->mem.base);
        //printf("Get Reg:%s\n", LynxReg2Str(parent));
    }

    if(op->mem.index != LYNX_INVALID) {
        curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.index, tid, curLabel);
        //LynxReg parent = LynxReg2FullLynxReg(op->mem.index);
        //printf("Get Reg:%s\n", LynxReg2Str(parent));
    }

    return curLabel;
}

uint64_t getOpTaint(TaintState *state, uint32_t tid, ReaderOp *op, uint64_t curLabel, uint8_t keepRBP) {
    if(op->type == REG_OP) {
        //if(op->reg != LYNX_GFLAGS && op->reg != LYNX_RIP && op->reg != LYNX_RSP) {
        if(op->reg != LYNX_GFLAGS) {
            curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->reg, tid, curLabel);
        }
    }
    else if(op->type == MEM_OP) {
        // TEST
        curLabel = getCombinedByteMemTaint(state->memTaint, state->labelState, op->mem.addr, op->mem.size, curLabel);

        if(op->mem.seg != LYNX_INVALID && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.seg, tid, curLabel);
        }

        if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.base, tid, curLabel);
        }

        if(op->mem.index != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP && keepRBP) {
            curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, op->mem.index, tid, curLabel);
        }
    }

    return curLabel;
}



uint64_t getAddrCalcListTaint(TaintState *state, uint32_t tid, ReaderOp *ops, uint32_t cnt, uint64_t curLabel, uint8_t keepRegInAddrCalc) {
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

uint8_t isPushIns(xed_iclass_enum_t inst) {
  uint8_t res = (inst == XED_ICLASS_PUSH
		 || inst == XED_ICLASS_PUSHA
		 || inst == XED_ICLASS_PUSHAD
		 || inst == XED_ICLASS_PUSHF
		 || inst == XED_ICLASS_PUSHFD
		 || inst == XED_ICLASS_PUSHFQ);
  return res;
}

uint8_t isPopIns(xed_iclass_enum_t inst) {
  uint8_t res = (inst == XED_ICLASS_POP
		 || inst == XED_ICLASS_POPA
		 || inst == XED_ICLASS_POPAD
		 || inst == XED_ICLASS_POPF
		 || inst == XED_ICLASS_POPFD
		 || inst == XED_ICLASS_POPFQ
		 || inst == XED_ICLASS_POPCNT);
  return res;
}

uint8_t isCallIns(xed_iclass_enum_t inst) {
  uint8_t res = (inst == XED_ICLASS_CALL_FAR
		 || inst == XED_ICLASS_CALL_NEAR
		 || inst == XED_ICLASS_SYSCALL
		 || inst == XED_ICLASS_SYSCALL_AMD);
  return res;
}

uint8_t isRetIns(xed_iclass_enum_t inst) {
  uint8_t res = (inst == XED_ICLASS_RET_FAR
		 || inst == XED_ICLASS_RET_NEAR
		 || inst == XED_ICLASS_IRET
		 || inst == XED_ICLASS_IRETD
		 || inst == XED_ICLASS_IRETQ
		 || inst == XED_ICLASS_SYSRET
		 || inst == XED_ICLASS_SYSRET_AMD);
  return res;
}

uint8_t canSkipTaintBecauseInsType(xed_iclass_enum_t inst){
  uint8_t isLeave = (inst == XED_ICLASS_LEAVE);
  uint8_t isEnter = (inst == XED_ICLASS_ENTER);
  return(isRetIns(inst)
	 || isPushIns(inst)
	 || isPopIns(inst)
	 || isCallIns(inst)
	 || isLeave
	 || isEnter);
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
        // If we have a ret/call/leave/enter/ect that reads taint from RSP/RIP, then ignore it (keepReg flag)
        if(!keepReg && canSkipTaintBecauseInsType(inst)){
          if(ops->type == REG_OP){
            LynxReg parent = LynxReg2FullLynxReg(ops->reg);
            if(parent == LYNX_RSP || parent == LYNX_RIP || (parent == LYNX_RBP && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
              ops = ops->next;
              continue;
            }
          } else if(ops->type == MEM_OP && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER)){
            keepRBP = 0;
          }
        }
        curLabel = getOpTaint(state, tid, ops, curLabel, keepRBP);
        ops = ops->next;
    }
    return curLabel;
}

uint64_t getTopLevelListTaint(TaintState *state, uint32_t tid, ReaderOp *ops, uint32_t cnt, uint64_t curLabel) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == REG_OP) {
            if(ops->reg != LYNX_GFLAGS && ops->reg != LYNX_RIP && ops->reg != LYNX_RSP) {
                curLabel = getCombinedByteRegTaint(state->regTaint, state->labelState, ops->reg, tid, curLabel);
            }
        }
        else if(ops->type == MEM_OP) {
            curLabel = getCombinedByteMemTaint(state->memTaint, state->labelState, ops->mem.addr, ops->mem.size, curLabel);
        }
        ops = ops->next;
    }

    return curLabel;
}

/**
 * set taint
 **/

void setAllMemTaint(TaintState *state, uint64_t addr, uint64_t size, uint64_t label) {
    if(size != 0) {
        setAllByteMemTaint(state->memTaint, addr, size, label);
    }
}

void setAllRegTaint(TaintState *state, uint32_t tid, LynxReg reg, uint64_t label) {
    LynxReg fullReg = getFullReg(state->readerState, reg);

    if(fullReg == LYNX_RIP || fullReg == LYNX_RSP || fullReg == LYNX_GFLAGS) {
        return;
    }

    setAllByteRegTaint(state->regTaint, reg, tid, label);
}

void setAllOpListTaint(TaintState *state,
		       uint32_t tid,
		       ReaderOp *ops,
		       uint32_t cnt,        /* no. of operands */
		       uint64_t combined,
		       uint8_t keepReg,
		       xed_iclass_enum_t inst) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == REG_OP) {
#if 0
            if(!keepReg && canSkipTaintBecauseInsType(inst)){
              if(ops->type == REG_OP){
                LynxReg parent = LynxReg2FullLynxReg(ops->reg);
                if(parent == LYNX_RSP || parent == LYNX_RIP || (parent == LYNX_RBP && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
                  if((parent == LYNX_RBP && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
                    //printf("We have a %s ins writing to %s\n", xed_iclass_enum_t2str(inst), LynxReg2Str(parent));
                  }
                  ops = ops->next;
                  continue;
                }
              }
            }
#endif
            int size = getRegSize(state->readerState, ops->reg);
            if (size == 4) {
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


void setAddrCalcListTaint(TaintState *state, uint32_t tid, ReaderOp *ops, uint32_t cnt, uint64_t label) {
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

void combineAddrCalcTaint(TaintState *state, uint32_t tid, ReaderOp *op, uint64_t combined) {
    if(op->mem.seg != LYNX_INVALID) {
        addTaintToByteReg(state->regTaint, state->labelState, op->mem.seg, tid, combined);
    }

    if(op->mem.base != LYNX_INVALID && op->mem.base != LYNX_RIP && op->mem.base != LYNX_RSP) {
        addTaintToByteReg(state->regTaint, state->labelState, op->mem.base, tid, combined);
    }

    if(op->mem.index != LYNX_INVALID && op->mem.index != LYNX_RIP && op->mem.index != LYNX_RSP) {
        addTaintToByteReg(state->regTaint, state->labelState, op->mem.index, tid, combined);
    }
}

void combineAddrCalcListTaint(TaintState *state, uint32_t tid, ReaderOp *ops, uint32_t cnt, uint64_t apply) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == MEM_OP) {
            combineAddrCalcTaint(state, tid, ops, apply);
        }
        ops = ops->next;
    }
}

void combineOpListTaint(TaintState *state, uint32_t tid, ReaderOp *ops, uint32_t cnt, uint64_t combined) {
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


/**
 * Forward Propagation
 **/

uint64_t getSrcTaint(TaintState *state, ReaderEvent *event, InsInfo *info, uint64_t curLabel, uint8_t keepRegInAddrCalc) {
    if(info->srcFlags != 0) {
        curLabel = getByteFlagTaint(state->regTaint, state->labelState, event->ins.tid, info->srcFlags, curLabel);
    }
    // See if our current ins has been written to CODE_GEN_DEPENDENCY
    curLabel = getCombinedByteMemTaint(state->memTaint, state->labelState, event->ins.addr, event->ins.binSize, curLabel);
    curLabel = getOpListTaint(state, event->ins.tid, info->srcOps, info->srcOpCnt, curLabel, keepRegInAddrCalc, info->insClass);
    curLabel = getOpListTaint(state, event->ins.tid, info->readWriteOps, info->readWriteOpCnt, curLabel, keepRegInAddrCalc, info->insClass);
    return getAddrCalcListTaint(state, event->ins.tid, info->dstOps, info->dstOpCnt, curLabel, keepRegInAddrCalc);
}

uint64_t defaultBytePropagation(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc) {
    uint64_t label = 0;
    label = getSrcTaint(state, event, info, label, keepRegInAddrCalc);

    setByteFlagTaint(state->regTaint, state->labelState, event->ins.tid, info->dstFlags, label);
    setAllOpListTaint(state, event->ins.tid, info->readWriteOps, info->readWriteOpCnt, label, keepRegInAddrCalc, info->insClass);
    setAllOpListTaint(state, event->ins.tid, info->dstOps, info->dstOpCnt, label, keepRegInAddrCalc, info->insClass);

    return label;
}

uint64_t xorBytePropagation(TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc) {
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
            setAllOpListTaint(state, event->ins.tid, info->readWriteOps, info->readWriteOpCnt, 0, keepRegInAddrCalc, info->insClass);
            setAllOpListTaint(state, event->ins.tid, info->dstOps, info->dstOpCnt, 0, keepRegInAddrCalc, info->insClass);
            return 0;
        }
    }
    else if(op1->type == MEM_OP && op2->type == REG_OP) {
        if(op1->mem.addr == op2->mem.addr && op1->mem.size == op2->mem.size) {
            setAllOpListTaint(state, event->ins.tid, info->readWriteOps, info->readWriteOpCnt, 0, keepRegInAddrCalc, info->insClass);
            setAllOpListTaint(state, event->ins.tid, info->dstOps, info->dstOpCnt, 0, keepRegInAddrCalc, info->insClass);
            return 0;
        }
    }

    return defaultBytePropagation(state, event, info, keepRegInAddrCalc);
}


void initByteHandlers(uint64_t (**handlers) (TaintState *state, ReaderEvent *event, InsInfo *info, uint8_t keepRegInAddrCalc)) {
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

/**
 * Backward Propagation
 **/

uint64_t getDstTaint(TaintState *state, uint32_t tid, InsInfo *info, uint64_t curLabel) {
    if(info->dstFlags != 0) {
        curLabel = getByteFlagTaint(state->regTaint, state->labelState, tid, info->dstFlags, curLabel);
    }
    
    curLabel = getTopLevelListTaint(state, tid, info->dstOps, info->dstOpCnt, curLabel);
    return getTopLevelListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, curLabel);
}

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

uint64_t backwardBytePropagationWithIns(TaintState *state, uint32_t tid, InsInfo *info, ReaderIns instruction) {
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
	taintMem(state, instruction.addr, instruction.binSize, label);// Taint memory region of instruction
        printf("Adding label %ld to taint mem location %lx in propagate backwards\n", label, instruction.addr);
        combineAddrCalcListTaint(state, tid, info->readWriteOps, info->readWriteOpCnt, label);
        combineOpListTaint(state, tid, info->srcOps, info->srcOpCnt, label);
        combineAddrCalcListTaint(state, tid, info->dstOps, info->dstOpCnt, label);

        return label;
    }

    return 0;
}
