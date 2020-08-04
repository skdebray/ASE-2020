/**
 * This file contains all of the code necessary to verify the consistency of various elements in the Reader.
 * Currently this includes making sure the reader and trace agree about the read and write operands for an
 * instruction and that they agree on the current architectural state when an instruction is executed.
 **/

#include <TraceFileHeader.h>
#include "Verify.h"

typedef enum {
    OP_READ = 1,
    OP_WRITE = 2,
} OpMark;

typedef enum {
    MEM_SEG = 4,
    MEM_BASE = 8, 
    MEM_INDEX = 16,
} MemRegMark; 

/**
 * Function: regInMem
 * Description: Checks to see if a register is one of the registers used to calculate the address of the 
 *  memory operand
 * Side Effects: If the register is used by the operand, it marks that the appropriate register in the 
 *  memory operand was found by modifying the op's marks
 * Output: 1 if found, 0 otherwise
 **/
int regInMem(LynxReg reg, ReaderOp *op) {
    if (op->type != MEM_OP) {
        return 0;
    }

    //need to mark all registers that match, and you can have base == index
    int found = 0;
    if (reg == op->mem.seg) {
        op->mark |= MEM_SEG;
        found = 1;
    }

    if (reg == op->mem.base) {
        op->mark |= MEM_BASE;
        found = 1;
    }

    if (reg == op->mem.index) {
        op->mark |= MEM_INDEX;
        found = 1;
    }

    return found;
}

/**
 * Function: regInMemOps
 * Description:  Iterates over cnt ops and checks each to see if the given reg is used by the memory operand.
 * Output: 1 if register is found, 0 otherwise
 **/
int regInMemOps(LynxReg reg, ReaderOp *ops, uint8_t cnt) {
    int i;
    int found = 0;
    for(i = 0; i < cnt; i++) {
        if (regInMem(reg, ops)) {
            found = 1;
        }
        ops = ops->next;
    }

    return found;
}

/**
 * Function: regInRegOps
 * Description: Iterates over cnt ops and checks to see if any of the REG_OPs use that register. If the 
 *  register is found, it marks the op with the mark given by the mark parameter.
 * Side Effects: Modifies the op's mark using the mark parameter if the op is found by oring the ops mark
 *  with the mark parameter
 * Output: 1 if found, 0 otherwise
 **/
int regInRegOps(LynxReg reg, ReaderOp *ops, uint8_t cnt, uint8_t mark) {
    int i;
    int found = 0;
    for(i = 0; i < cnt; i++) {
        if ((ops->type == REG_OP && reg == ops->reg)) {
            ops->mark |= mark;
            found = 1;
        }
        ops = ops->next;
    }

    return found;
}

/**
 * Function: regInOps
 * Description: Iterates over cnt ops and checks if the given reg is used by any of the mem ops or reg ops.
 *  If used by a reg op, the op's mark is or'd with the mark parameter. If used by a memory op, then the 
 *  mark for the appropriate register is or'd with the op's mark.
 * Side Effects: Will modify the op's mark if the given register is found in either a reg op or a mem op.
 * Output: 1 if found, 0 otherwise
 **/
int regInOps(LynxReg reg, ReaderOp *ops, uint8_t cnt, uint8_t mark) {
    int i;
    int found = 0;
    for(i = 0; i < cnt; i++) {
        if ((ops->type == REG_OP && reg == ops->reg)) {
            ops->mark |= mark;
            found = 1;
        }
        else if (regInMem(reg, ops)) {
            found = 1;
        }
        ops = ops->next;
    }

    return found;
}

/**
 * Function: memInOps
 * Description: Iterates over cnt ops and checks if there is a memory operand with a matching address. If
 *  such an address is found, the mark given by the mark parameter is or'd with the op's mark
 * Side Effects: The mark of any memory op with the matching address is modified
 * Output: 1 if found, 0 otherwise
 **/
int memInOps(uint64_t addr, uint16_t size, ReaderOp *ops, uint8_t cnt, uint8_t mark) {
    int i;
    int found = 0;
    for(i = 0; i < cnt; i++) {
        if (ops->type == MEM_OP && addr == ops->mem.addr && size == ops->mem.size) {
            ops->mark |= mark;
            found = 1;
        }
    }

    return found;
}

/**
 * Function: initializeMarks
 * Description: Initializes the marks of all of the ops in InsInfo to 0
 * Side Effects: Modifies all marks of InsInfo ops
 * Output: None
 **/
void initializeMarks(InsInfo *info) {
    if (info != NULL) {
        int i;
        ReaderOp *op = info->srcOps;
        for(i = 0; i < info->srcOpCnt; i++) {
            op->mark = 0;
            op = op->next;
        }

        op = info->dstOps;
        for(i = 0; i < info->dstOpCnt; i++) {
            op->mark = 0;
            op = op->next;
        }

        op = info->readWriteOps;
        for(i = 0; i < info->readWriteOpCnt; i++) {
            op->mark = 0;
            op = op->next;
        }
    }
}

/**
 * Function: getMarkStr
 * Description: Returns a string with the appropriate read/write status of the mark
 * Output: String containing whether the mark indicates if operand can be read or written
 **/
const char *getMarkStr(uint8_t targetMark) {
    //shortcut
    if (targetMark == (OP_READ | OP_WRITE)) {
        return "read or written";
    }
    if (targetMark == OP_READ) {
        return "read";
    }
    if (targetMark == OP_WRITE) {
        return "written";
    }

    return "Unknown";
}

/**
 * Function: checkOp
 * Description: Checks to see if the ops have all of the marks required to be considered consistent with
 *  the trace. The targetMark parameter gives the marks we want to check for (i.e. read/write) and memory
 *  will also be check to ensure all of the register marks are present as well. The selections are also taken
 *  into account when checking marks, so if source registers are not included in the selections, they won't
 *  be checked.
 * Side Effects: Outputs warning messages 
 * Output: None
 **/
void checkOp(ReaderOp *op, char *mnemonic, uint8_t targetMark, uint16_t selections) {
    if (op->type == REG_OP && op->reg != LYNX_RIP && op->reg != LYNX_EIP) {
        int checkMark = ((targetMark & OP_READ) &&  (op->reg != LYNX_GFLAGS) 
            && (selections & (1 << SEL_SRCREG))) ? OP_READ : 0;

        checkMark |= ((targetMark & OP_WRITE) && (selections & (1 << SEL_DESTREG))) ? OP_WRITE : 0;

        if ((op->mark & checkMark) != checkMark) {
            char msg[256];
            sprintf(msg, "Register %s from %s not %s in trace", LynxReg2Str(op->reg), mnemonic, 
                getMarkStr(targetMark & ~op->mark));

            throwWarning(msg);
        }
    }
    else if (op->type == MEM_OP) {
        int checkSrcRegs = selections & SEL_SRCREG;
        int checkMark = ((targetMark & OP_READ) && (selections & (1 << SEL_MEMREAD))) ? OP_READ : 0;
        checkMark |= ((targetMark & OP_WRITE) && (selections & (1 << SEL_MEMWRITE))) ? OP_WRITE : 0;

        if (op->mem.seg != LYNX_INVALID && checkSrcRegs && (op->mark & MEM_SEG) == 0) {
            char msg[256];
            sprintf(msg, "Segment register %s from %s used in xed but not trace", 
                LynxReg2Str(op->mem.seg), mnemonic);

            throwWarning(msg);
        }
        if (op->mem.base != LYNX_INVALID && checkSrcRegs && (op->mark & MEM_BASE) == 0) {
            char msg[256];
            sprintf(msg, "Base register %s from %s used in xed but not trace", 
                LynxReg2Str(op->mem.base), mnemonic);

            throwWarning(msg);
        }
        if (op->mem.index != LYNX_INVALID && checkSrcRegs && (op->mark & MEM_INDEX) == 0) {
            char msg[256];
            sprintf(msg, "Index register %s from %s used in xed but not trace", 
                LynxReg2Str(op->mem.index), mnemonic);

            throwWarning(msg);
        }
        if (op->mem.addrGen == 0 && (op->mark & checkMark) != checkMark) {
            char msg[256];
            sprintf(msg, "Memory at %llx from %s not %s in trace", 
                (unsigned long long) op->mem.addr, mnemonic, getMarkStr(targetMark & ~op->mark));

            throwWarning(msg);
        }
    }
}

/**
 * Function: checkMarks
 * Description: Will check all of the marks to ensure that the Reader and Trace agree on the source and 
 *  destination operands.
 * Side Effects: Outputs warnings if an inconsistency is found
 * Output: None
 **/
void checkMarks(InsInfo *info, uint16_t selections) {
    int i;
    ReaderOp *op = info->srcOps;
    for(i = 0; i < info->srcOpCnt; i++) {
        checkOp(op, info->mnemonic, OP_READ, selections);
        op->mark = 0;
        op = op->next;
    }

    op = info->dstOps;
    for(i = 0; i < info->dstOpCnt; i++) {
        checkOp(op, info->mnemonic, OP_WRITE, selections);
        op->mark = 0;
        op = op->next;
    }

    op = info->readWriteOps;
    for(i = 0; i < info->readWriteOpCnt; i++) {
        checkOp(op, info->mnemonic, OP_READ | OP_WRITE, selections);
        op->mark = 0;
        op = op->next;
    }
}

/**
 * Function: checkVals
 * Description: Checks to see if size bytes of val1 and val2 match. If not, throws a warning with msg
 * Side Effects: Prints a warning if inconsistent
 * Assumptions: Both val1 and val2 are at least size bytes
 * Output: 1 if consistent, 0 otherwise
 **/
int checkVals(const uint8_t *val1, const uint8_t *val2, uint16_t size, const char *msg) {
    int i;
    for(i = 0; i < size; i++) {
        if (val1[i] != val2[i]) {
            throwWarning(msg);
            return 0;
        }
    }

    return 1;
}

/**
 * Function: findDstReg
 * Description: Tries to find reg in the destination registers
 * Side Effects: Prints a message if reg is not found
 * Output: 1 if found, 0 otherwise
 **/
int findDstReg(LynxReg reg, InsInfo *info) {
    int found = regInRegOps(reg, info->readWriteOps, info->readWriteOpCnt, OP_WRITE);
    found = regInRegOps(reg, info->dstOps, info->dstOpCnt, OP_WRITE) || found;

    if (!found) {
        char msg[256];
        sprintf(msg, "Register %s from %s written in trace but not xed", LynxReg2Str(reg), info->mnemonic);
        throwWarning(msg);
        return 0;
    }

    return 1;
}

/**
 * Function: findSrcReg
 * Description: Tries to find the source register reg. It searches source ops, readWriteops and the 
 *  source registers of memory writes.
 * Side Effects: Prints a message if reg is not found
 * Output: 1 if found, 0 otherwise
 **/
int findSrcReg(LynxReg reg, InsInfo *info) {
    //mark all as found
    int found = regInOps(reg, info->srcOps, info->srcOpCnt, OP_READ);
    found = regInOps(reg, info->readWriteOps, info->readWriteOpCnt, OP_READ) || found;
    found = regInMemOps(reg, info->dstOps, info->dstOpCnt) || found;
    if (!found) {
        char msg[256];
        sprintf(msg, "Register %s from %s read in trace but not xed", LynxReg2Str(reg), info->mnemonic);
        throwWarning(msg);
        return 0;
    }

    return 1;
}

/**
 * Function: findDstMem
 * Description: Tries to find a memory write operand with the matching address
 * Side Effects: Prints a message if matching memory write wasn't found
 * Output: 1 if found, 0 otherwise
 **/
int findDstMem(uint64_t addr, uint16_t size, InsInfo *info) {
    int found = memInOps(addr, size, info->readWriteOps, info->readWriteOpCnt, OP_WRITE);
    found = memInOps(addr, size, info->dstOps, info->dstOpCnt, OP_WRITE) || found;

    if (!found) {
        char msg[256];
        sprintf(msg, "Memory at %llx from %s written in trace but not xed", 
            (unsigned long long) addr, info->mnemonic);

        throwWarning(msg);
        return 0;
    }

    return 1;
}

/**
 * Function: findSrcMem
 * Description: Tries to find a memory read operand with the matching address
 * Side Effects: Prints a message if matching memory read wasn't found
 * Output: 1 if found, 0 otherwise
 **/
int findSrcMem(uint64_t addr, uint16_t size, InsInfo *info) {
    //must find all memory with the address
    int found = memInOps(addr, size, info->srcOps, info->srcOpCnt, OP_READ);
    found = memInOps(addr, size, info->readWriteOps, info->readWriteOpCnt, OP_READ) || found;

    if (!found) {
        char msg[256];
        sprintf(msg, "Memory at %llx from %s read in trace but not xed", 
            (unsigned long long) addr, info->mnemonic);

        throwWarning(msg);
        return 0;
    }

    return 1;
}

