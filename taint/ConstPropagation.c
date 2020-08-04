
#include <assert.h>
#include "ByteRegTaint.h"
#include "ByteMemoryTaint.h"
#include "LabelStore.h"
#include "TaintState.h"
#include "Taint.h"
#include <Reader.h>

/**
 * Taint Fetching
 **/
uint64_t areAndFlagsConst(TaintState *state, uint32_t tid, uint32_t flags) {
    uint32_t curFlag = 1;
    const uint64_t *flagTaint = getByteRegTaint(state->regTaint, LYNX_GFLAGS, tid);

    int i;
    for(i = 0; i < 32; i++) {
        if(flags & curFlag && flagTaint[i] == 0) {
            return 0;
        }
        curFlag <<= 1;
    }

    return 1;
}

uint64_t areOrFlagsConst(TaintState *state, uint32_t tid, uint32_t flags) {
    if(flags == 0) {
        return 1;
    }

    uint32_t curFlag = 1;
    const uint64_t *flagTaint = getByteRegTaint(state->regTaint, LYNX_GFLAGS, tid);

    int i;
    for(i = 0; i < 32; i++) {
        if(flags & curFlag && flagTaint[i] != 0) {
            return 1;
        }
        curFlag <<= 1;
    }

    return 0;
}

uint64_t isRegConst(TaintState *state, uint32_t tid, LynxReg reg) {
    if(reg == LYNX_RIP || reg == LYNX_RSP || reg == LYNX_GFLAGS) {
        return 1;
    }

    uint32_t size = getRegSize(state->readerState, reg);
    const uint64_t *taint = getByteRegTaint(state->regTaint, reg, tid);

    int i;
    for(i = 0; i < size; i++) {
        if(taint[i] == 0) {
            return 0;
        }
    }

    return 1;
}

uint64_t isMemConst(TaintState *state, uint64_t addr, uint32_t size) {
    uint64_t taint[64];

    getByteMemTaint(state->memTaint, addr, size, taint);

    int i;
    for(i = 0; i < size; i++) {
        if(taint[i] == 0) {
            return 0;
        }
    }
    
    return 1;
}

uint64_t isAddrCalcConst(TaintState *state, uint32_t tid, ReaderOp *op) {
    if(op->mem.seg != LYNX_INVALID) {
        if(!isRegConst(state, tid, op->mem.seg)) {
            return 0;
        }
    }

    if(op->mem.base != LYNX_INVALID) {
        if(!isRegConst(state, tid, op->mem.base)) {
            return 0;
        }
    }

    if(op->mem.index != LYNX_INVALID) {
        if(!isRegConst(state, tid, op->mem.index)) {
            return 0;
        }
    }

    return 1;
}

uint64_t isOpConst(TaintState *state, ReaderEvent *event, ReaderOp *op) {
    if(op->type == REG_OP) {
        return isRegConst(state, event->ins.tid, op->reg);
    }
    else if(op->type == MEM_OP) {
        if(!isMemConst(state, op->mem.addr, op->mem.size)) {
            return 0;
        }

        return isAddrCalcConst(state, event->ins.tid, op);
    }

    return 1;
}

uint64_t isOpListConst(TaintState *state, ReaderEvent *event, ReaderOp *ops, uint32_t cnt) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        if(!isOpConst(state, event, ops)) {
            return 0;
        }
        ops = ops->next;
    }

    return 1;
}

uint64_t isAddrCalcListConst(TaintState *state, ReaderEvent *event, ReaderOp *ops, uint32_t cnt) {
    int i;
    for(i = 0; i < cnt; i++) {
        if(ops->type == MEM_OP && !isAddrCalcConst(state, event->ins.tid, ops)) {
            return 0;
        }
        ops = ops->next;
    }

    return 1;
}

/**
 * set const
 **/
void setFlagsConst(TaintState *state, uint32_t tid, uint32_t flags, uint64_t label) {
    uint32_t curFlag = 1;
   
    int i; 
    for(i = 0; i < 32; i++) {
        if(flags & curFlag) {
            setByteRegTaintLoc(state->regTaint, LYNX_GFLAGS, tid, i, label);
        }
        curFlag <<= 1;
    }
}

void setAddrCalcConst(TaintState *state, uint32_t tid, ReaderOp *op, uint64_t label) {
    if(op->mem.seg != LYNX_INVALID) {
        setAllByteRegTaint(state->regTaint, op->mem.seg, tid, label);
    }

    if(op->mem.base != LYNX_INVALID) {
        setAllByteRegTaint(state->regTaint, op->mem.base, tid, label);
    }

    if(op->mem.index != LYNX_INVALID) {
        setAllByteRegTaint(state->regTaint, op->mem.index, tid, label);
    }
}

void setOpConst(TaintState *state, ReaderEvent *event, ReaderOp *op, uint64_t label) {
    if(op->type == REG_OP) {
        LynxReg reg = op->reg;

        //x86_64 weirdness: 
        //https://stackoverflow.com/questions/11177137/why-do-most-x64-instructions-zero-the-upper-part-of-a-32-bit-register
        if(getRegSize(state->readerState, reg) == 4) {
            reg = getFullReg(state->readerState, reg);
        }

        if(reg != LYNX_GFLAGS) {
            setAllByteRegTaint(state->regTaint, reg, event->ins.tid, label);
        }
    }
    else if(op->type == MEM_OP) {
        setAllByteMemTaint(state->memTaint, op->mem.addr, op->mem.size, label);
    }
}

void setOpListConst(TaintState *state, ReaderEvent *event, ReaderOp *ops, uint32_t cnt, uint64_t label) {
    uint32_t i;
    for(i = 0; i < cnt; i++) {
        setOpConst(state, event, ops, label);
        ops = ops->next;
    }
}


/**
 * Forward Propagation
 **/

uint8_t propagateConst(TaintState *state, ReaderEvent *event, InsInfo *info) {
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
                setOpListConst(state, event, info->readWriteOps, info->readWriteOpCnt, 1); 
                setOpListConst(state, event, info->dstOps, info->dstOpCnt, 1); 
                return 1;
            }   
        }   
        else if(op1->type == MEM_OP && op2->type == REG_OP) {
            if(op1->mem.addr == op2->mem.addr && op1->mem.size == op2->mem.size) {
                setOpListConst(state, event, info->readWriteOps, info->readWriteOpCnt, 1); 
                setOpListConst(state, event, info->dstOps, info->dstOpCnt, 1); 
                return 1;
            }   
        }
    }

    if(areOrFlagsConst(state, event->ins.tid, info->srcFlags) && isOpListConst(state, event, info->srcOps, info->srcOpCnt) && isOpListConst(state, event, info->readWriteOps, info->readWriteOpCnt) && isAddrCalcListConst(state, event, info->dstOps, info->dstOpCnt)) {
        setFlagsConst(state, event->ins.tid, info->dstFlags, 1);
        setOpListConst(state, event, info->readWriteOps, info->readWriteOpCnt, 1);
        setOpListConst(state, event, info->dstOps, info->dstOpCnt, 1);

        return 1;
    }

    setFlagsConst(state, event->ins.tid, info->dstFlags, 0);
    setOpListConst(state, event, info->readWriteOps, info->readWriteOpCnt, 0);
    setOpListConst(state, event, info->dstOps, info->dstOpCnt, 0);

    return 0;
}

