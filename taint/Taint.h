#ifndef __TAINT_H_
#define __TAINT_H_

#include <stdint.h>
#include <Reader.h>

typedef struct TaintState_t TaintState;

/*******************************************************************************
 *                                                                             *
 *                               INITIALIZATION                                *
 *                                                                             *
 *******************************************************************************/

/*
 * initTaint() -- initializes a taint propagation state.  This function should be
 * called at the beginning of any taint analysis.  The code below initializes the
 * taint propagation functions for byte-level propagation; if a different policy is
 * needed (e.g., bit-level or word-level), these function pointers should be set 
 * appropriately.
 */
TaintState *initTaint(ReaderState *readerState);

/*******************************************************************************
 *                                                                             *
 *                                  CLEAN-UP                                   *
 *                                                                             *
 *******************************************************************************/

/*
 * freeMemTaintLabels() -- resets the state of the shadow memory to be clean.
 */
void freeMemTaintLabels(TaintState *state);
void freeTaint(TaintState *state);
void recoverSpace(TaintState *state, uint8_t *alive);

/*******************************************************************************
 *                                                                             *
 *                              TAINT INTRODUCTION                             *
 *                                                                             *
 *******************************************************************************/

/*
 * taintOperand() -- apply the taint label label to operand op in thread-id tid.
 */
int taintOperand(TaintState *state,
		 ReaderOp *op,
		 uint32_t tid,
		 uint64_t label,
		 InsInfo *info);

/*
 * taintOperandList() -- apply the taint label label to the set of operands ops 
 * in thread-id tid.
 */
int taintOperandList(TaintState *state,
		     ReaderOp *ops,
		     int cnt,
		     uint32_t tid,
		     uint64_t label,
		     InsInfo *info);

/*
 * taintIns() -- propagates taint originating from a particular instruction. 
 * To do so it  taints all of the instruction writes (i.e., the read-write 
 * operands and  the destination operands) together with the instruction bytes. 
 * The instruction bytes are also tainted so as to handle self-modifying code.
 */
int taintIns(TaintState *state, ReaderEvent *event, InsInfo *info, uint64_t label);

/*
 * taintReg() -- set the taint of register reg in the thread specified to label.
 */
int taintReg(TaintState *state, LynxReg reg, uint32_t thread, uint64_t label);

/*
 * taintMem() -- set the taint of size bytes of memory, starting at address addr,
 * to label.
 */
int taintMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label);

/*
 * taintMemBlock() -- set the taint of size bytes of memory, starting at address
 * addr, to be the array of taint labels given by labels.
 */
int taintMemBlock(TaintState *state, uint64_t addr, uint32_t size, uint64_t *labels);

/*
 * taintAddrCalc() -- adds taint to the address computation of a memory operand.
 * Given an operand op: if op is a memory operand, this function sets the taint
 * label for each register used for the address calculation to label, and returns
 * the value 1; if op is not a memory operand, it leaves taint unchanged and
 * returns the value 0.
 */
int taintAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);

/*
 * taintAddrCalcList() -- given a set of operands ops, adds taint to all their
 * address calculations.  Returns 1 if some address calculation register was
 * affected, 0 otherwise.
 */
int taintAddrCalcList(TaintState *state,
		      ReaderOp *ops,
		      int cnt,
		      uint32_t tid,
		      uint64_t label);

uint8_t propagateConst(TaintState *state, ReaderEvent *event, InsInfo *info);

/*
 * addTaintToReg() -- add the taint in label to that of register reg in thread tid.
 */
int addTaintToReg(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label);

/*
 * addTaintToMem() -- adds the taint in label to the per-byte taint in a memory
 * region of size bytes starting at address addr.
 */
int addTaintToMem(TaintState *state, uint64_t addr, uint32_t size, uint64_t label);

/*
 * addTaintToOperand() -- Add the taint in label to operand op:
 *
 *    -- if op is a register reg other than RIP or RSP, add taint to the reg.
 *    -- if op is a memory operand, taint the memory locations it addresses
 *              as well as any registers used in the address computation.
 */
int addTaintToOperand(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);

/*
 * addTaintToOperandList() -- Add the taint in label to a set of operands ops.
 */
int addTaintToOperandList(TaintState *state,
			  ReaderOp *ops,
			  int cnt,
			  uint32_t tid,
			  uint64_t label);

/*
 * addTaintToAddrCalc() -- given an operand op and a taint label, if op is a memory
 * operand it adds the taint specified by label to any register that is used for 
 * its memory address calculation; in this case it returns the value 1.  If op is
 * not a memory operand there is no effect, and the value returned is 0.
 */
int addTaintToAddrCalc(TaintState *state, ReaderOp *op, uint32_t tid, uint64_t label);

/*
 * addTaintToAddrCalcList() -- given a set of operands ops and a taint label, adds
 * the taint label to any register used for an address computation in ops.  The 
 * return value is 1 if any such register was encountered, 0 otherwise.
 */
int addTaintToAddrCalcList(TaintState *state,
			   ReaderOp *ops,
			   int cnt,
			   uint32_t tid,
			   uint64_t label);

/*******************************************************************************
 *                                                                             *
 *                              TAINT PROPAGATION                              *
 *                                                                             *
 *******************************************************************************/

/*
 * getNewLabel() -- create and return a new taint label.
 */
uint64_t getNewLabel(TaintState *state);

/*
 * propagateForward() -- propagate taint forward through a reader event (i.e.,
 * instruction), with corresponding InsInfo structure info (see documentation
 * on the trace reader for more about ReaderEvent and InsInfo structures).
 */
uint64_t propagateForward(TaintState *state,
			  ReaderEvent *event,
			  InsInfo *info,
			  uint8_t keepRegInAddrCalc);

/*
 * propagateBackward() -- propagate taint backward through an instruction 
 * indicated by info and thread tid.
 */
uint64_t propagateBackward(TaintState *state, uint32_t tid, InsInfo *info);

/*
 * propagateBackwardWithIns() -- propagate taint backward through instruction
 * with corresponding InsInfo info (similarly to propagateBackward), and
 * additionally taint the bytes occupied by instruction.  Tainting of the
 * instruction bytes aims to deal with self-modifying code.
 */
uint64_t propagateBackwardWithIns(TaintState *state,
				  uint32_t tid,
				  InsInfo *info,
				  ReaderIns instruction);

/*******************************************************************************
 *                                                                             *
 *                                TAINT OUTPUT                                 *
 *                                                                             *
 *******************************************************************************/

void getCondensedLabels(TaintState *state);
uint8_t *getLabelArray(TaintState *state);
void getArchLabels(TaintState *state, uint8_t *labels);
uint8_t *outputCondensedLabels(TaintState *state, uint8_t *labels);

/*
 * outputTaint() -- print out tainted registers and memory locations,
 * together with their taint labels, in the given taint state.
 */
void outputTaint(TaintState *state);


uint64_t getCombinedRegTaint(TaintState *state, LynxReg reg, uint32_t tid, uint64_t label);

uint64_t popLabel(TaintState *state, uint64_t *label);
uint64_t subtractLabel(TaintState *state, uint64_t label, uint64_t from);
uint8_t labelIsSubsetOf(TaintState *state, uint64_t label, uint64_t in);
uint64_t labelSize(TaintState *state, uint64_t label);
uint64_t mergeLabels(TaintState *state, uint64_t l1, uint64_t l2);
uint64_t getFirstLabel(TaintState *state, uint64_t label);
uint64_t getLastLabel(TaintState *state, uint64_t label);
uint64_t getLabelIntersect(TaintState *state, uint64_t l1, uint64_t l2);
uint64_t *getSubLabels(TaintState *state, uint64_t label);
void printBaseLabels(TaintState *state, uint64_t label);
uint8_t hasLabelInRange(TaintState *state, uint64_t label, uint64_t min, uint64_t max);
uint8_t labelHasSequential(TaintState *state, uint64_t world, uint64_t label);
uint64_t getMemTaintLabel(TaintState *state, uint64_t addr, uint32_t sizeInBytes, uint64_t init);
uint64_t getLabelArraySize(TaintState *state);
#endif
