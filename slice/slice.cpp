/*
 * File: slice.cpp
 *
 * This code implements the backward backward slicing algorithm described in:
 *
 *    B. Korel, Computation of Dynamic Program Slices for Unstructured Programs,
 *    IEEE Transactions on Software Engineering vol. 23 no. 1, Jan. 1997,
 *    pages 17-34.
 *
 */

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <cassert>
#include <unordered_set>
#include <algorithm>
#include <utility>
#include <chrono>
#include <iostream>
#include "slice.h"
#include "sliceState.h"
#include <libgen.h>
#include <xed-interface.h>
#include <xed-iclass-enum.h>
#include "utils.h"

using namespace std;
using namespace std::chrono;
using std::vector;
using std::unordered_set;

extern "C" {
    #include "../shared/LynxReg.h"
    #include <Reader.h>
    #include <Taint.h>
    #include <cfg.h>
    #include <cfgAPI.h>
    #include <cfgState.h>
    #include <controlTransfer.h>
}

/*
 *  Function: getActionAtIndex
 *  Purpose: Return action at given index from trace
 */
Action *getActionAtIndex(SliceState *slice, uint64_t index){
  return(slice->listOfActions[index]);
}

uint64_t getAddrFromAction(Action *action){
  if(action->instruction->event.type == EXCEPTION_EVENT){
    return(action->instruction->event.exception.addr);
  }
  return(action->instruction->event.ins.addr);
}

/*******************************************************************************
 *                                                                             *
 * isCmpIns, isPushIns, isPopIns: checking for specific x86 instruction types  *
 *                                                                             *
 *******************************************************************************/

int isCmpIns(Action *action){
  xed_iclass_enum_t inst = action->curInfo->insClass;
  return (inst == XED_ICLASS_TEST || inst == XED_ICLASS_CMP);
}

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

/*
*  Function: getBlockSlice
*  Purpose: Given a action, use the action's cfg pointer to get the single block that 
*           holds all the actions
*/
blockSlice *getBlockSlice(SliceState *slice, Action *action){
  if(slice->htableslice.find(action->instruction) != slice->htableslice.end()){
    return(slice->htableslice.find(action->instruction)->second);
  }
  return NULL;
}

/*
*  Function: createNewBlock
*  Purpose: Make a new Korel-block for a given set of indicies
*/
blockSlice *createNewBlock(SliceState *slice, uint64_t startPos, uint64_t endPos, uint64_t id){
  blockSlice *bbl = new blockSlice();
  bbl->start = startPos;
  bbl->end = endPos;
  bbl->id = id;
  assert(getActionAtIndex(slice, startPos)->tid == getActionAtIndex(slice, endPos)->tid);
  uint32_t tid = getActionAtIndex(slice, startPos)->tid;
  uint64_t i;
  for(i = startPos; i <= endPos; i++){
    if(getActionAtIndex(slice, i)->tid != tid){
      continue;
    }
    // Update action's block pointer
    getActionAtIndex(slice, i)->bbl = bbl;
    // Update block's collection of actions
    bbl->actionsSet.insert(i);
    // We use an actions cfgInstruction pointer to map to that action's block
    slice->htableslice[getActionAtIndex(slice, i)->instruction] = bbl;
  }
  return(bbl);
}

uint64_t getNextOfSameThread(SliceState *slice, uint64_t pos, uint32_t tid){
  while((pos < slice->numActions) && (getActionAtIndex(slice, pos)->tid != tid)){
    pos++;
  }
  return pos;
}

bool isCmpJumpBlock(SliceState *slice, uint64_t pos){
  Action *act;
  uint64_t numActions = slice->numActions;
  bool isACmp = isCmpIns(getActionAtIndex(slice, pos));
  bool isInRange = (pos+1 < numActions);

  if(!isInRange || !isACmp){
    return false;
  }

  act = getActionAtIndex(slice, pos);
  uint64_t nextThreadInsPos = getNextOfSameThread(slice, pos+1, act->tid);

  if(nextThreadInsPos >= numActions){
    return false;
  }

  act = getActionAtIndex(slice, nextThreadInsPos);
  bool nextInsInThreadIsJump = isConditionalJump(act->curInfo->insClass);

  return (isACmp && nextInsInThreadIsJump);
}

bool brokenCompJumpBlock(SliceState *slice, uint64_t pos){
  bool firstHalfSeen = (getBlockSlice(slice, getActionAtIndex(slice, pos)) != NULL
			&& isCmpIns(getActionAtIndex(slice, pos)));
  if(!firstHalfSeen){ // bail out early
    return false;
  }
  blockSlice *nextJmpBBLInThread = getBlockSlice(slice,
						 getActionAtIndex(slice, getNextOfSameThread(slice, pos+1, getActionAtIndex(slice, pos)->tid)));
  bool secondHalfNotSeen = (firstHalfSeen && nextJmpBBLInThread == NULL);
  bool secondHalfIsNotMatch = (secondHalfNotSeen
			       || (getBlockSlice(slice, getActionAtIndex(slice, pos)) != nextJmpBBLInThread));
  return(secondHalfIsNotMatch);
}

/*
*  Function: buildBlocks
*  Purpose: For a given collection of actions, build a list of blocks with those actions
*/
void buildBlocks(SliceState *slice){
  // Build list of blocks from list of actions
  uint64_t numActions = slice->numActions;
  uint64_t i, blockCounter;
  
  for(i = 0, blockCounter = 0; i < numActions; i++){
    // Two cases to handle. Either we have created a block for the action(s) and should just update or still need to create a new block
    // CASE 1: Create a new block: two types of blocks (normal 1 ins block or cmp + jmp block)
    //         The second type of block is trickier to tell (i.e. both actions must been seen to update em)
    if(getBlockSlice(slice, getActionAtIndex(slice, i)) == NULL
       || brokenCompJumpBlock(slice, i)){ // If we haven't seen this block before
      // If-goto block (actions at i,i+1 go into one block)
      if(isCmpJumpBlock(slice, i)){
        uint64_t nextJump = getNextOfSameThread(slice, i+1, getActionAtIndex(slice, i)->tid);
        blockSlice *bbl = createNewBlock(slice, i, nextJump, blockCounter);
        slice->removableBlocks.insert(bbl);
        // Small Opt: if both cmp/jmp instructions in same thread then can skip looking at next ins
        if(i+1 == nextJump){
          i++; // Just inserted two actions into our block
        }
      } 
      // Regular block (1 instruction for now)
      else {
        blockSlice *bbl = createNewBlock(slice, i, i, blockCounter);
        slice->removableBlocks.insert(bbl);
      }
      blockCounter++;
    } else { // We've seen this block before, update which actions are in that block
      // cmp + jump block (when we've seen both before IN SAME BLOCK)
      if(isCmpJumpBlock(slice, i)){
        // Add the compare
        uint64_t nextJump = getNextOfSameThread(slice, i+1, getActionAtIndex(slice, i)->tid);
        getBlockSlice(slice, getActionAtIndex(slice, i))->actionsSet.insert(i);
        getActionAtIndex(slice, i)->bbl = getBlockSlice(slice, getActionAtIndex(slice, i));
        // Then update the jmp
        getBlockSlice(slice, getActionAtIndex(slice, nextJump))->actionsSet.insert(nextJump);
        getActionAtIndex(slice, nextJump)->bbl = getBlockSlice(slice, getActionAtIndex(slice, nextJump));
        // Small Opt: if both cmp/jmp instructions in same thread then can skip looking at next ins
        if(i+1 == nextJump){
          i++; // Just inserted two actions into our block
        }
      } 
      // Otherwise standard action that we've seen/built a block for before
      else {
        getBlockSlice(slice, getActionAtIndex(slice, i))->actionsSet.insert(i);
        getActionAtIndex(slice, i)->bbl = getBlockSlice(slice, getActionAtIndex(slice, i));
      }
    }
  }
}

void setAllNotMarkedOrVisited(SliceState *slice){
  uint64_t numActions = slice->numActions;
  uint64_t i;
  for(i = 0; i < numActions; i++){
    getActionAtIndex(slice, i)->visited = 0;
    getActionAtIndex(slice, i)->marked = 0;
  }
}

void collectLastDefinitionsUsingCurAction(TaintState *backTaint,
					  Action *curAction,
					  std::map<uint64_t, Action *> &mapLabelToAction,
					  uint8_t keepReg){
  // Forward Propagate any taint
  uint64_t taint = propagateForward(backTaint, &(curAction->instruction->event), curAction->curInfo, keepReg);
  // Walk through all labels to find last definitions
  uint64_t size = labelSize(backTaint, taint);
  uint64_t *labelsInTaint = getSubLabels(backTaint, taint);
  for(uint64_t i = 0; i < size; i++){
    uint64_t label = labelsInTaint[i];
    if(mapLabelToAction.find(label) != mapLabelToAction.end()){
      Action *found = mapLabelToAction[label];

      // For PUSH, look for reg last def
      if(isPushIns(curAction->curInfo->insClass)){
        uint64_t regLabel = 0;
        regLabel = getCombinedRegTaint(backTaint, curAction->reg, curAction->tid, regLabel);
        if(regLabel == label){
          curAction->lastDefForRegOnPush = found;
        }
      }
      // For POP, set up the bypass last def
      if(found->bypass == NULL){
        curAction->lastDefList.push_back(found);
      } else {
        // If we have a POP as the last def and its a save-restore reg (i.e. found->bypass is a thing)
        // then reroute last def. Only way a pop can be last def is for a given register
        curAction->lastDefList.push_back(found->bypass);
      }
    }
  }
}

void taintRegOps(TaintState *backTaint,
		 ReaderOp *op,
		 Action *curAction,
		 std::map<uint64_t, Action *> &mapLabelToAction,
		 uint64_t actionLabel){
  // Get parent reg from op->reg
  LynxReg parent = LynxReg2FullLynxReg(op->reg);
  taintReg(backTaint,(LynxReg) parent, curAction->tid, actionLabel);
}

void taintMemOps(TaintState *backTaint,
		 ReaderOp *op,
		 Action *curAction,
		 std::map<uint64_t, Action *> &mapLabelToAction,
		 uint64_t actionLabel){
  if(op->type != MEM_OP){
    return;
  }
  taintMem(backTaint, op->mem.addr, op->mem.size, actionLabel);
}

void taintRegMemOps(TaintState *backTaint,
		    ReaderOp *op,
		    Action *curAction,
		    std::map<uint64_t, Action *> &mapLabelToAction,
		    uint64_t actionLabel){
  if(op->type == REG_OP){
    taintRegOps(backTaint, op, curAction, mapLabelToAction, actionLabel);
  } else if(op->type == MEM_OP){
    taintMemOps(backTaint, op, curAction, mapLabelToAction, actionLabel);
  }
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
  uint8_t isRIPUser = (isConditionalJump(inst)
		       || isUnconditionalJump(inst));
  return(isRet || isCall || isLeave || isEnter || isPushIns(inst) || isPopIns(inst) || isRIPUser);
}

void taintDests(TaintState *backTaint,
		Action *curAction,
		std::map<uint64_t, Action *> &mapLabelToAction,
		uint8_t keepReg){

  // Get a new label to apply to all dest/sources
  uint64_t actionLabel = getNewLabel(backTaint);
  mapLabelToAction[actionLabel] = curAction;
  InsInfo *info = curAction->curInfo;

  // Walk through source ops
  ReaderOp *op = info->srcOps;

  // Now go through read/write ops
  op = info->readWriteOps;
  for(int i = 0; i < info->readWriteOpCnt; i++){
    // ignore ret/call/leave/enter/etc. that reads taint from RSP/RIP (keepReg flag)
    xed_iclass_enum_t inst = info->insClass;
    if(!keepReg && canSkipTaintBecauseInsType(inst)){
      if(op->type == REG_OP){
        LynxReg parent = LynxReg2FullLynxReg(op->reg);
        if(parent == LYNX_RSP
	   || parent == LYNX_RIP
	   || (parent == LYNX_RBP
	       && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
          op = op->next; // HANDLE: RSP cases
          continue;
        }
      }
    }
    taintRegMemOps(backTaint, op, curAction, mapLabelToAction, actionLabel);
    op = op->next;
  }

  // Finally dstOps
  op = info->dstOps;
  for(int i = 0; i < info->dstOpCnt; i++){
    // Ignore ret/call/leave/enter/ect that reads taint from RSP/RIP (keepReg flag)
    xed_iclass_enum_t inst = info->insClass;
    if(!keepReg && canSkipTaintBecauseInsType(inst)){
      if(op->type == REG_OP){
        LynxReg parent = LynxReg2FullLynxReg(op->reg);
        if(parent == LYNX_RSP
	   || parent == LYNX_RIP
	   || (parent == LYNX_RBP
	       && (inst == XED_ICLASS_LEAVE || inst == XED_ICLASS_ENTER))){
          op = op->next;
          continue;
        }
      }
    }
    taintRegMemOps(backTaint, op, curAction, mapLabelToAction, actionLabel);
    op = op->next;
  }
}

void handlePush(Action *curAction, map<uint32_t, map<LynxReg, Action *>> &mapTIDToPushMap){
  mapTIDToPushMap[curAction->tid][curAction->reg] = curAction;
}

int handlePop(Action *curAction, map<uint32_t, map<LynxReg, Action *>> &mapTIDToPushMap){
  if(mapTIDToPushMap[curAction->tid].find(curAction->reg) == mapTIDToPushMap[curAction->tid].end()){
    return 0;
  }
  Action *push = mapTIDToPushMap[curAction->tid][curAction->reg];
  if(push->rspVal == curAction->rspVal && push->reg == curAction->reg){
    const uint8_t *pushVal = push->regVal;
    const uint8_t *popVal = curAction->regVal;
    int sizePush = LynxRegSize(push->reg);
    int sizePop = LynxRegSize(curAction->reg);
    if(pushVal == NULL || popVal == NULL){
      return 0;
    }
    if(sizePush != sizePop){
      return 0;
    }
    for(int walk = 0; walk < sizePush; walk++){
      if(pushVal[walk] != popVal[walk]){
        return 0;
      }
    }
    return 1;
  }
  return 0;
}

void findAllLastDefinitions(SliceState *slice, Action *action, uint8_t keepReg){
  TaintState *backTaint = slice->taintState;
  std::map<uint64_t, Action *> mapLabelToAction;
  std::map<uint32_t, map<LynxReg, Action *>> mapTIDToPushMap;

  // Walk backwards through trace from slice starting position
  // Find all last definitions along the way for every action
  uint64_t position = action->position;
  uint64_t walk = 0;
  //printf("Starting\n");
  while(walk <= position){
    action = getActionAtIndex(slice, walk);
    if(action->instruction->event.type != EXCEPTION_EVENT){
      if(!keepReg && isPushIns(action->curInfo->insClass)){
        handlePush(action, mapTIDToPushMap);
      }
      // Add current action to all appropriate last def lists
      collectLastDefinitionsUsingCurAction(backTaint, action, mapLabelToAction, keepReg);
      // Taint destinations, any instructions that end up having these as sources trace back the taint to this ins
      uint8_t haveSaveRestorePair = 0;
      if(!keepReg && isPopIns(action->curInfo->insClass)){
        haveSaveRestorePair = handlePop(action, mapTIDToPushMap);
        // If we have a save restore pair then set up the bypass in last defs
        if(haveSaveRestorePair){
          action->bypass = mapTIDToPushMap[action->tid][action->reg]->lastDefForRegOnPush;
        }
      }
      taintDests(backTaint, action, mapLabelToAction, keepReg);
    }
    walk++;    // move to next action
  }
}

void markAction(SliceState *slice, Action *action){
  slice->numMarked++;
  action->marked = 1;
  slice->marked.insert(action);
}

void visitAction(SliceState *slice, Action *action){
  slice->numVisited++;
  action->visited = 1;
  slice->visited.insert(action);
}

void markRemainingActions(SliceState *slice,
			  uint64_t posi,
			  set<Action *> contributing,
			  set<Action *> noncontributing){
  uint64_t i;
  for(i = 0; i < posi; i++){
    if(contributing.find(getActionAtIndex(slice, i)) == contributing.end()
       && noncontributing.find(getActionAtIndex(slice, i)) == noncontributing.end()){
      markAction(slice, getActionAtIndex(slice, i));
    }
  }
}

void findContributing(SliceState *slice, set<Action *> &markedAndNotVisited){
  while(!markedAndNotVisited.empty()){
    Action *current = *markedAndNotVisited.begin();
    slice->visited.insert(current);
    slice->contributing.insert(current);

    // Go through and mark all actions in lastDef list
    for(Action *lastDef : current->lastDefList){
      markAction(slice, lastDef);
      // Optimization to add into markedAndNotVisited WITHOUT a set_diff
      // If this action is not yet visited, add into markedAndNotVisited set
      if(slice->visited.find(lastDef) == slice->visited.end()){
        markedAndNotVisited.insert(lastDef);
      } else {
        slice->contributing.insert(lastDef);
      }
    }

    // For all blocks, see if action is a part-of the block and remove block from set if so
    //blockSlice *block = getBlockSlice(slice, getActionAtIndex(slice, current->position));
    blockSlice *block = current->bbl;
    slice->removableBlocks.erase(block);

    // At this point current has been visited
    markedAndNotVisited.erase(current);
  }
}

bool isREntry(SliceState *slice, Action *action){
  // Check if previous action was a jump
  if(action->position >= 1){
    xed_iclass_enum_t prevIns = getActionAtIndex(slice, action->position-1)->curInfo->insClass;
    if((isConditionalJump(prevIns)
	&& !(StraightLineCode(getActionAtIndex(slice, action->position-1)->instruction->event.ins,
			      action->instruction->event.ins)))
       || isUnconditionalJump(prevIns)){
      return false;
    }
  }
  // Now see if this action is at a start of a block
  // NOTE: We are using the cfg instruction as many actions can map to one cfgInstruction
  // Actions can be duplicated, cfgInstructions are not
  if(getActionAtIndex(slice, action->bbl->start)->instruction == action->instruction){
    return true;
  }
  return false;
}

bool isRExit(SliceState *slice, Action *exitAction){
  uint64_t position = exitAction->position;
  Action *endOfBlock = getActionAtIndex(slice, exitAction->bbl->end);
  if(exitAction->instruction != endOfBlock->instruction){
    return false;
  }
  if(isUnconditionalJump(exitAction->curInfo->insClass)){
    return false;
  } else if(isConditionalJump(exitAction->curInfo->insClass)
	    && (position+1 < slice->numActions)
	    && !(StraightLineCode(exitAction->instruction->event.ins,
				  (getActionAtIndex(slice, position+1)->instruction->event.ins)))){
    return false;
  }
  return true;
}

// RExit is instruction immediately after a block ends,
// where the previous action is not a jump
Action *findNearestRExit(SliceState *slice, Action *from, uint64_t stop){
  uint64_t position = from->position;
  Action *nearestRExitAct = NULL;
  // Search for exit
  Action *exitActionTest = NULL;
  position++;
  while(position < stop){
    exitActionTest = getActionAtIndex(slice, position);
    // Check if its a jump exit
    if(isRExit(slice, exitActionTest)){
      nearestRExitAct = exitActionTest;
    }
    position++;
  }
  if(nearestRExitAct == NULL){
    return NULL;
  }
  // Otherwise return instruction that immediately follows
  return getActionAtIndex(slice, nearestRExitAct->position+1);
}

set<Action *> findNonContributing(SliceState *slice, uint64_t sliceSpot){
  set<Action *> nonContributing;
  uint64_t posi = 0;
  while (posi < sliceSpot){
    Action *current = getActionAtIndex(slice, posi);
    // If our action isn't in the contributing set and is at an RENTRY
    if(slice->contributing.find(current) == slice->contributing.end() && isREntry(slice, current)){
      // Find nearest contributing action
      uint64_t nearestContribPos = posi+1;
      while(slice->contributing.find(getActionAtIndex(slice, nearestContribPos)) == slice->contributing.end()){
        nearestContribPos++;
      }
      // If there is a nearest contributing action
      if(nearestContribPos > posi && nearestContribPos <= sliceSpot){
        // Grab it. The position it resides at is walk
        Action *contributingNearest = getActionAtIndex(slice, nearestContribPos);
        // We know that current is at an R entry (see above)
        // Try to find nearest r-exit between our selected r-entry and the contributing action
        Action *rexit = findNearestRExit(slice, current, nearestContribPos);
        // If we found a nearest r-exit and if its position is before t
        set<Action *> temp;
        uint64_t nonContribWalk = posi;
        // Now check if there is a closest rexit of current->bbl before our contributing action
        if(rexit != NULL && rexit->position <= contributingNearest->position){
          // Add Instructions to temp set
          while(nonContribWalk < rexit->position){ // grabz
            nonContributing.insert(getActionAtIndex(slice, nonContribWalk));
            nonContribWalk++;
          }
          posi = nonContribWalk - 1;
        }
      }
    }
    posi++;
  }
  return(nonContributing);
}

void printLastDefs(SliceState *slice, uint64_t numActions){
  for(uint64_t i = 0; i < numActions; i++){
    Action *temp = getActionAtIndex(slice, i);
    if(!temp->lastDefList.empty()){
      printf("Last definitions of %lx are: ", getAddrFromAction(temp));
      for(Action *temps : temp->lastDefList){
        printf("%lx in phase %d ", getAddrFromAction(temps), temps->instruction->block->fun->cfgPhase->id);
      }
      printf("\n");
    }
  }
}


/*******************************************************************************
 *                                                                             *
 * build_action_list() -- construct a list (array) of actions corresponding to *
 * the trace.  This list is traversed when computing the slice.                *
 *                                                                             *
 *******************************************************************************/

void build_action_list(SlicedriverState *driver_state) {
  bool slice_addr_match, slice_pos_match;
  Action **listOfActions = (Action **) malloc(driver_state->numIns * sizeof(Action *));
  if (listOfActions == NULL) {
    fprintf(stderr, "[%s] Not enough memory\n", __func__);
    exit(1);
  }

  uint64_t position = 0;

  driver_state->slice_start_action = NULL;
  driver_state->num_actions = 0;
  driver_state->listOfActions = listOfActions;

  for (std::pair<cfgInstruction *, infoTuple *> collection : *(driver_state->insCollection)) {
    cfgInstruction *instruction = (std::get<0>(collection));
    infoTuple *savedInfo = (std::get<1>(collection));
    InsInfo *curInfo = savedInfo->storedInfo;

    Action *this_action = new Action();
    this_action->position = position;
    this_action->instruction = instruction;
    this_action->instruction->keep = 2;
    this_action->keep = 2;
    this_action->curInfo = curInfo;
    this_action->bbl = NULL;
    this_action->bypass = NULL;
    this_action->tid = savedInfo->tid;
      
    // Handle push/pops
    xed_iclass_enum_t inst = curInfo->insClass;
    if (!driver_state->keepReg
	&& instruction->event.type != EXCEPTION_EVENT
	&& (isPopIns(inst) || isPushIns(inst))) {
      this_action->regVal = savedInfo->regVal;
      this_action->rspVal = savedInfo->rspVal;
      this_action->reg = savedInfo->reg;
    }
    
    listOfActions[position] = this_action;
    position++;

    if (instruction->event.type == INS_EVENT) {
      slice_addr_match = (driver_state->sliceAddr != 0
			  && instruction->event.ins.addr == driver_state->sliceAddr);
      slice_pos_match = (driver_state->slice_pos != 0
			 && savedInfo->event_pos == driver_state->slice_pos);
      if (slice_addr_match || slice_pos_match) {
	/*
	 * if we matched on slice position, and the slice address was also specified,
	 * check that the address at the matched position is consistent with the
	 * specified slice address.
	 */
	if (slice_pos_match) {
	  if (driver_state->sliceAddr != 0 && !slice_addr_match) {
	    fprintf(stderr,
		    "ERROR: Slice address 0x%lx does not match instruction address (0x%lx) at position %d\n",
		    driver_state->sliceAddr,
		    instruction->event.ins.addr,
		    (int)driver_state->slice_pos);
	    exit(1);
	  }
	}
	driver_state->slice_start_action = this_action;
	driver_state->num_actions = position;
      }
    }
  }

  return;
}


/*******************************************************************************
 *                                                                             *
 * compute_slice() -- computes a backward dynamic slice using information in   *
 * the argument driver_state.                                                  *
 *                                                                             *
 *******************************************************************************/

SliceState *compute_slice(SlicedriverState *driver_state){
  build_action_list(driver_state);

  if (driver_state->slice_start_action == NULL) {
    fprintf(stderr, "Could not find action at address 0x%lx\n", driver_state->sliceAddr); 
    exit(1);
  }
    
  ReaderState *rState = driver_state->rState;
  TaintState *backTaint = driver_state->backTaint;
  uint64_t numActions = driver_state->num_actions;
  Action **listOfActions = driver_state->listOfActions;
  Action *startOfSlice = driver_state->slice_start_action;

  SliceState *slice = new SliceState();
  slice->listOfActions = listOfActions;
  slice->numActions = numActions;
  slice->readerState = rState;
  slice->taintState = backTaint;

  buildBlocks(slice);
  setAllNotMarkedOrVisited(slice);
  findAllLastDefinitions(slice, startOfSlice, driver_state->keepReg);

  // Mark the start of our slice (its not visited yet though)
  markAction(slice, startOfSlice);

  // Get our set of marked + not visited actions
  set<Action *> markedAndNotVisited;
  set<Action *> nonContributing;
  markedAndNotVisited.insert(startOfSlice);

  // loop until there does not exist a marked and not visited action
  uint64_t iterationsOuter = 0;
  do {
    findContributing(slice, markedAndNotVisited);
    nonContributing = findNonContributing(slice, startOfSlice->position);
    markRemainingActions(slice, startOfSlice->position, slice->contributing, nonContributing); 
    // Set subtraction of marked - visited = how many marked + nonvisited actions we have
    markedAndNotVisited.clear();
    set_difference(slice->marked.begin(), slice->marked.end(), slice->visited.begin(), slice->visited.end(), inserter(markedAndNotVisited, markedAndNotVisited.end()));
    iterationsOuter++;
  } while(!markedAndNotVisited.empty());

  // Print non contributing
  uint64_t remove = 0;
  for (blockSlice *block : slice->removableBlocks){
    for(uint64_t actPosition : block->actionsSet){
      Action *act = getActionAtIndex(slice, actPosition);
      act->keep = 0;
      remove++;
      if(act->instruction->event.type != EXCEPTION_EVENT){
        if(slice->contributing.find(act) != slice->contributing.end()){
          printf("CONFLICT %lx %s is both in contributing and removable!\n",
		 getAddrFromAction(act), act->curInfo->mnemonic);
          exit(1);
        }
      }
    }
  }

  uint64_t save = 0;
  for(uint64_t i = 0; i < numActions; i++){
    Action *act = getActionAtIndex(slice, i);
    if(act->keep == 2){
      act->instruction->keep = 1;
      act->keep = 1;
      slice->saveableCFGInstructions.insert(act->instruction);
      save++;
      if(driver_state->validate && act->instruction->event.type != EXCEPTION_EVENT){
        printf("%ld; %lx; %s;\n",
	       act->position,
	       getAddrFromAction(act),
	       act->instruction->block->fun->name);
      }
    } else if(act->instruction->keep != 1) {
      act->instruction->keep = 0;
      slice->removableCFGInstructions.insert(act->instruction);
    }
  }
  
#if 0
  fprintf(driver_state->outf, "%ld removable actions\n", remove);
  fprintf(driver_state->outf, "%ld contributing actions\n", save);
  fprintf(driver_state->outf,
	  "%ld removable cfgInstructions\n",
	  slice->removableCFGInstructions.size());
  fprintf(driver_state->outf,
	  "%ld saveable cfgInstructions\n",
	  slice->saveableCFGInstructions.size());
  fprintf(driver_state->outf,
	  "%ld total cfgInstructions\n",
	  slice->saveableCFGInstructions.size() + slice->removableCFGInstructions.size());
  fprintf(driver_state->outf,
	  "%ld iterations of outer loop\n\n",
	  iterationsOuter);
#endif
  
  print_slice_instrs(slice, driver_state);
  
  return(slice);
}

